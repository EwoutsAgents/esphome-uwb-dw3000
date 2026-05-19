#include "tag_driver.h"

#include <cmath>
#include <cstring>

#include "dw3000.h"
#include "dw3000_regs.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uwb_dw3000 {

static const char *const TAG = "uwb_dw3000.tag_driver";

namespace {

constexpr uint16_t TX_ANT_DLY = 16385;
constexpr uint16_t RX_ANT_DLY = 16385;
constexpr uint8_t ALL_MSG_COMMON_LEN = 10;
constexpr uint8_t ALL_MSG_SN_IDX = 2;
constexpr uint8_t RESP_MSG_POLL_RX_TS_IDX = 10;
constexpr uint8_t RESP_MSG_RESP_TX_TS_IDX = 14;
constexpr uint8_t RX_BUF_LEN = 24;
constexpr uint16_t POLL_TX_TO_RESP_RX_DLY_UUS_DEFAULT = (100 + CPU_COMP);
constexpr uint16_t RESP_RX_TIMEOUT_UUS = 700;
constexpr uint16_t UWB_PRE_TIMEOUT = 50;
constexpr float A_IPATOV_64 = 121.7f;
constexpr uint32_t INIT_IDLE_TIMEOUT_MS = 1500;
constexpr uint32_t TX_COMPLETE_TIMEOUT_MS = 200;

#ifndef DGC_DBG_ID
#define DGC_DBG_ID 0x30060UL
#endif
#define DGC_DBG_DGC_DECISION_BIT_OFFSET (28U)
#define DGC_DBG_DGC_DECISION_BIT_MASK (0x7UL << DGC_DBG_DGC_DECISION_BIT_OFFSET)

TagDriverPins g_pins{4, 34, 27};
uint8_t g_tag_id = 0x45;
uint8_t g_frame_seq_nb = 0;
bool g_first_loop = false;
uint16_t g_poll_tx_to_resp_rx_dly_uus = POLL_TX_TO_RESP_RX_DLY_UUS_DEFAULT;

uint8_t tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 0x00, 'V', 0x00, 0xE0, 0, 0};
uint8_t rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 0x00, 'W', 0x00, 0xE1,
                         0,    0,    0, 0,    0,    0,    0,    0,    0,    0};
uint8_t rx_buffer[RX_BUF_LEN];
uint32_t status_reg = 0;

dwt_sts_cp_key_t cp_key = {0x14EB220F, 0xF86050A8, 0xD1D336AA, 0x14148674};
dwt_sts_cp_iv_t cp_iv = {0x1F9A3DE4, 0xD37EC3CA, 0xC44FA8FB, 0x362EEB34};

void clear_rx_tx_status_() {
  dwt_write32bitreg(SYS_STATUS_ID,
                    SYS_STATUS_ALL_RX_GOOD | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR |
                        SYS_STATUS_TXFRS_BIT_MASK);
}

void reset_rx_tx_state_() {
  dwt_forcetrxoff();
  clear_rx_tx_status_();
}

float safe_log10f(float x) {
  if (x <= 0.0f)
    return -300.0f;
  return log10f(x);
}

uint8_t read_dgc_decision_() {
  const uint32_t dgc_dbg = dwt_read32bitreg(DGC_DBG_ID);
  return static_cast<uint8_t>((dgc_dbg & DGC_DBG_DGC_DECISION_BIT_MASK) >>
                              DGC_DBG_DGC_DECISION_BIT_OFFSET);
}

float compute_fp_power_dbm_(const dwt_rxdiag_t &d, uint8_t dgc) {
  if (d.ipatovAccumCount == 0)
    return -300.0f;
  const float f1 = static_cast<float>(d.ipatovF1);
  const float f2 = static_cast<float>(d.ipatovF2);
  const float f3 = static_cast<float>(d.ipatovF3);
  const float n = static_cast<float>(d.ipatovAccumCount);
  const float raw = (f1 * f1 + f2 * f2 + f3 * f3) / (n * n);
  return 10.0f * safe_log10f(raw) + 6.0f * static_cast<float>(dgc) - A_IPATOV_64;
}

float compute_rx_power_dbm_(const dwt_rxdiag_t &d, uint8_t dgc) {
  if (d.ipatovAccumCount == 0 || d.ipatovPower == 0)
    return -300.0f;
  const float c = static_cast<float>(d.ipatovPower);
  const float n = static_cast<float>(d.ipatovAccumCount);
  const float raw = c * static_cast<float>(1U << 21) / (n * n);
  return 10.0f * safe_log10f(raw) + 6.0f * static_cast<float>(dgc) - A_IPATOV_64;
}

void fill_cir_metrics_(const dwt_rxdiag_t &diag, TagDriverCirMetrics *metrics) {
  if (metrics == nullptr)
    return;

  const uint8_t dgc = read_dgc_decision_();
  const float fp = compute_fp_power_dbm_(diag, dgc);
  const float rx = compute_rx_power_dbm_(diag, dgc);
  const float ratio = fp - rx;

  metrics->fp_power_db = fp;
  metrics->rx_power_db = rx;
  metrics->cir_ratio_db = ratio;
  metrics->dgc_decision = dgc;

  if (ratio > -2.0f) {
    metrics->status = 1;
  } else if (ratio < -6.0f) {
    metrics->status = 2;
  } else {
    metrics->status = 0;
  }

  float fp_lin = powf(10.0f, fp / 10.0f);
  float rx_lin = powf(10.0f, rx / 10.0f);
  float nlos_lin = rx_lin - fp_lin;
  if (nlos_lin < 1e-9f)
    nlos_lin = 1e-9f;
  metrics->nlos_power_db = 10.0f * safe_log10f(nlos_lin);
}

bool send_tx_poll_msg_(uint32_t tx_mode) {
  tx_poll_msg[ALL_MSG_SN_IDX] = g_frame_seq_nb;
  clear_rx_tx_status_();
  dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
  dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);
  ESP_LOGD(TAG, "anchor=0x%02X tx_start seq=%u", tx_poll_msg[5], g_frame_seq_nb);

  const int ret = dwt_starttx(tx_mode);
  if (ret != DWT_SUCCESS) {
    const uint32_t status = dwt_read32bitreg(SYS_STATUS_ID);
    ESP_LOGW(TAG, "anchor=0x%02X outcome=tx_start_failed status=0x%08lX", tx_poll_msg[5],
             static_cast<unsigned long>(status));
    reset_rx_tx_state_();
    return false;
  }

  const uint32_t start = millis();
  uint32_t status = 0;
  while (!((status = dwt_read32bitreg(SYS_STATUS_ID)) & SYS_STATUS_TXFRS_BIT_MASK)) {
    if (millis() - start > TX_COMPLETE_TIMEOUT_MS) {
      ESP_LOGW(TAG, "anchor=0x%02X outcome=tx_timeout status=0x%08lX", tx_poll_msg[5],
               static_cast<unsigned long>(status));
      reset_rx_tx_state_();
      return false;
    }
    delay(1);
  }
  ESP_LOGD(TAG, "anchor=0x%02X tx_done status=0x%08lX", tx_poll_msg[5],
           static_cast<unsigned long>(status));
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
  return true;
}

}  // namespace

void uwb_tag_driver_set_pins(const TagDriverPins &pins) { g_pins = pins; }

void uwb_tag_driver_set_tag_id(uint8_t tag_id) { g_tag_id = tag_id; }

void uwb_tag_driver_set_rx_after_tx_delay_uus(uint16_t delay_uus) {
  g_poll_tx_to_resp_rx_dly_uus = delay_uus;
}

bool uwb_tag_driver_init() {
  UART_init();
  spiBegin(g_pins.irq_pin, g_pins.rst_pin);
  spiSelect(g_pins.ss_pin);
  delay(2);

  const uint32_t init_start = millis();
  while (!dwt_checkidlerc()) {
    if (millis() - init_start > INIT_IDLE_TIMEOUT_MS)
      return false;
    delay(1);
  }

  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
    return false;
  }

  dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

  if (dwt_configure(&config_options)) {
    return false;
  }

  dwt_or8bitoffsetreg(DGC_CFG_ID, 0x0, static_cast<uint8_t>(DGC_CFG_RX_TUNE_EN_BIT_MASK));

  if (config_options.chan == 5)
    dwt_configuretxrf(&txconfig_options);
  else
    dwt_configuretxrf(&txconfig_options_ch9);

  dwt_setrxantennadelay(RX_ANT_DLY);
  dwt_settxantennadelay(TX_ANT_DLY);
  set_resp_rx_timeout(RESP_RX_TIMEOUT_UUS, &config_options);
  dwt_setpreambledetecttimeout(UWB_PRE_TIMEOUT);
  dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);
  dwt_configciadiag(DW_CIA_DIAG_LOG_ALL);
  ESP_LOGCONFIG(TAG,
                "DW3000 config: chan=%u preamble=%u pac=%u txCode=%u rxCode=%u sfdType=%u "
                "dataRate=%u stsMode=%u stsLen=%u",
                config_options.chan, config_options.txPreambLength, config_options.rxPAC,
                config_options.txCode, config_options.rxCode, config_options.sfdType,
                config_options.dataRate, config_options.stsMode, config_options.stsLength);
  return true;
}

float uwb_tag_driver_range(uint8_t anchor_id, TagDriverCirMetrics *metrics) {
  tx_poll_msg[5] = anchor_id;
  tx_poll_msg[7] = g_tag_id;
  rx_resp_msg[7] = anchor_id;
  rx_resp_msg[5] = g_tag_id;

  if (!g_first_loop) {
    dwt_configurestskey(&cp_key);
    dwt_configurestsiv(&cp_iv);
    dwt_configurestsloadiv();
    g_first_loop = true;
  } else {
    dwt_writetodevice(STS_IV0_ID, 0, 4, reinterpret_cast<uint8_t *>(&cp_iv));
    dwt_configurestsloadiv();
  }

  ESP_LOGD(TAG, "anchor=0x%02X rx_enable delay_uus=%u timeout_uus=%u", anchor_id,
           g_poll_tx_to_resp_rx_dly_uus, RESP_RX_TIMEOUT_UUS);
  dwt_setrxaftertxdelay(g_poll_tx_to_resp_rx_dly_uus);
  dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

  if (!send_tx_poll_msg_(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED)) {
    return NAN;
  }

  uint32_t wait_start = millis();
  while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) &
           (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {
    if (millis() - wait_start > 1000) {
      reset_rx_tx_state_();
      ESP_LOGW(TAG, "anchor=0x%02X outcome=rx_wait_timeout", anchor_id);
      return NAN;
    }
    delay(1);
  }

  int16_t sts_qual = 0;
  const int good_sts = dwt_readstsquality(&sts_qual);
  g_frame_seq_nb++;

  float distance = NAN;
  const bool rx_good = (status_reg & SYS_STATUS_RXFCG_BIT_MASK) != 0;

  if (rx_good && (good_sts >= 0)) {
    const uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
    if (frame_len <= sizeof(rx_buffer)) {
      dwt_readrxdata(rx_buffer, frame_len, 0);
      rx_buffer[ALL_MSG_SN_IDX] = 0;

      if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0) {
        dwt_rxdiag_t diag;
        dwt_readdiagnostics(&diag);
        fill_cir_metrics_(diag, metrics);

        uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
        int32_t rtd_init, rtd_resp;
        float clock_offset_ratio;

        poll_tx_ts = dwt_readtxtimestamplo32();
        resp_rx_ts = dwt_readrxtimestamplo32();
        clock_offset_ratio = ((float) dwt_readclockoffset()) / (uint32_t) (1 << 26);

        resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
        resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

        rtd_init = resp_rx_ts - poll_tx_ts;
        rtd_resp = resp_tx_ts - poll_rx_ts;

        const float tof = ((rtd_init - rtd_resp * (1 - clock_offset_ratio)) / 2.0f) *
                          DWT_TIME_UNITS;
        distance = tof * SPEED_OF_LIGHT;
      }
      else {
        ESP_LOGW(TAG, "anchor=0x%02X outcome=bad_frame_header", anchor_id);
      }
    } else {
      ESP_LOGW(TAG, "anchor=0x%02X outcome=bad_frame_len len=%u", anchor_id,
               static_cast<unsigned>(frame_len));
    }
  } else if (!rx_good) {
    if ((status_reg & SYS_STATUS_ALL_RX_TO) != 0) {
      if ((status_reg & SYS_STATUS_RXPTO_BIT_MASK) != 0) {
        ESP_LOGW(TAG, "anchor=0x%02X outcome=rx_preamble_timeout status=0x%08lX", anchor_id,
                 static_cast<unsigned long>(status_reg));
      } else if ((status_reg & SYS_STATUS_RXFTO_BIT_MASK) != 0) {
        ESP_LOGW(TAG, "anchor=0x%02X outcome=rx_frame_timeout status=0x%08lX", anchor_id,
                 static_cast<unsigned long>(status_reg));
      } else if ((status_reg & SYS_STATUS_RXSTO_BIT_MASK) != 0) {
        ESP_LOGW(TAG, "anchor=0x%02X outcome=rx_sfd_timeout status=0x%08lX", anchor_id,
                 static_cast<unsigned long>(status_reg));
      } else {
        ESP_LOGW(TAG, "anchor=0x%02X outcome=rx_timeout_other status=0x%08lX", anchor_id,
                 static_cast<unsigned long>(status_reg));
      }
    } else {
      ESP_LOGW(TAG, "anchor=0x%02X outcome=rx_error status=0x%08lX", anchor_id,
               static_cast<unsigned long>(status_reg));
    }
  } else {
    ESP_LOGW(TAG, "anchor=0x%02X outcome=bad_sts sts_qual=%d status=0x%08lX", anchor_id,
             static_cast<int>(sts_qual), static_cast<unsigned long>(status_reg));
  }

  if (std::isfinite(distance)) {
    ESP_LOGD(TAG, "anchor=0x%02X outcome=ok distance=%.3f", anchor_id, distance);
  }

  if (std::isfinite(distance)) {
    clear_rx_tx_status_();
  } else {
    reset_rx_tx_state_();
  }

  return distance;
}

}  // namespace uwb_dw3000
}  // namespace esphome
