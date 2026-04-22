#include "dw3000_backend.h"

namespace {

dw3000_spi_begin_fn_t g_begin_fn = nullptr;
dw3000_spi_transfer_fn_t g_transfer_fn = nullptr;
dw3000_spi_end_fn_t g_end_fn = nullptr;

}  // namespace

extern "C" {

void dw3000_set_spi_backend(dw3000_spi_begin_fn_t begin_fn,
                            dw3000_spi_transfer_fn_t transfer_fn,
                            dw3000_spi_end_fn_t end_fn) {
  g_begin_fn = begin_fn;
  g_transfer_fn = transfer_fn;
  g_end_fn = end_fn;
}

bool dw3000_spi_backend_is_ready(void) { return g_begin_fn != nullptr && g_transfer_fn != nullptr && g_end_fn != nullptr; }

void dw3000_spi_backend_begin(void) {
  if (g_begin_fn != nullptr)
    g_begin_fn();
}

uint8_t dw3000_spi_backend_transfer(uint8_t data) {
  if (g_transfer_fn != nullptr)
    return g_transfer_fn(data);
  return 0;
}

void dw3000_spi_backend_end(void) {
  if (g_end_fn != nullptr)
    g_end_fn();
}

}  // extern "C"
