import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_IRQ_PIN,
    CONF_MODE,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_NAME,
    CONF_NUMBER,
    CONF_RST_PIN,
    CONF_SPI_ID,
    CONF_TRIGGER_ID,
    CONF_UPDATE_INTERVAL,
)

AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

CONF_SS_PIN = "ss_pin"
CONF_TAG_ID = "tag_id"
CONF_ROLE = "role"
CONF_ANCHORS = "anchors"
CONF_TAG_HEIGHT = "tag_height"
CONF_ANCHOR_ID = "anchor_id"
CONF_X = "x"
CONF_Y = "y"
CONF_Z = "z"

CONF_DISTANCE = "distance"
CONF_DISTANCE_FILTERED = "distance_filtered"
CONF_X_RAW = "x_raw"
CONF_Y_RAW = "y_raw"
CONF_X_FILTERED = "x_filtered"
CONF_Y_FILTERED = "y_filtered"
CONF_FP_POWER = "fp_power"
CONF_RX_POWER = "rx_power"
CONF_CIR_RATIO = "cir_ratio"
CONF_NLOS_POWER = "nlos_power"
CONF_STATUS = "status"

uwb_dw3000_ns = cg.esphome_ns.namespace("uwb_dw3000")
UwbDw3000Component = uwb_dw3000_ns.class_("UwbDw3000Component", cg.PollingComponent)
AnchorConfig = uwb_dw3000_ns.struct("AnchorConfig")

anchor_schema = cv.Schema(
    {
        cv.Required(CONF_ID): cv.hex_uint8_t,
        cv.Required(CONF_X): cv.float_,
        cv.Required(CONF_Y): cv.float_,
        cv.Required(CONF_Z): cv.float_,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UwbDw3000Component),
            cv.Required(CONF_ROLE): cv.one_of("tag", lower=True),
            cv.Required(CONF_SS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_RST_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_TAG_ID, default=0x45): cv.hex_uint8_t,
            cv.Required(CONF_ANCHORS): cv.All(cv.ensure_list(anchor_schema), cv.Length(min=3)),
            cv.Optional(CONF_TAG_HEIGHT, default=0.78): cv.float_,
            cv.Optional(CONF_UPDATE_INTERVAL, default="200ms"): cv.update_interval,
        }
    )
    .extend(cv.polling_component_schema("200ms"))
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    ss_pin = await cg.gpio_pin_expression(config[CONF_SS_PIN])
    irq_pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    rst_pin = await cg.gpio_pin_expression(config[CONF_RST_PIN])

    cg.add(var.set_ss_pin(ss_pin))
    cg.add(var.set_irq_pin(irq_pin))
    cg.add(var.set_rst_pin(rst_pin))
    cg.add(var.set_tag_id(config[CONF_TAG_ID]))
    cg.add(var.set_tag_height(config[CONF_TAG_HEIGHT]))

    for anchor in config[CONF_ANCHORS]:
        cg.add(
            var.add_anchor(
                anchor[CONF_ID], anchor[CONF_X], anchor[CONF_Y], anchor[CONF_Z]
            )
        )


sensor_schema = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(UwbDw3000Component),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement="m", accuracy_decimals=3, state_class="measurement"
        ).extend({cv.Required(CONF_ANCHOR_ID): cv.hex_uint8_t}),
        cv.Optional(CONF_DISTANCE_FILTERED): sensor.sensor_schema(
            unit_of_measurement="m", accuracy_decimals=3, state_class="measurement"
        ).extend({cv.Required(CONF_ANCHOR_ID): cv.hex_uint8_t}),
        cv.Optional(CONF_X_RAW): sensor.sensor_schema(
            unit_of_measurement="m", accuracy_decimals=3, state_class="measurement"
        ),
        cv.Optional(CONF_Y_RAW): sensor.sensor_schema(
            unit_of_measurement="m", accuracy_decimals=3, state_class="measurement"
        ),
        cv.Optional(CONF_X_FILTERED): sensor.sensor_schema(
            unit_of_measurement="m", accuracy_decimals=3, state_class="measurement"
        ),
        cv.Optional(CONF_Y_FILTERED): sensor.sensor_schema(
            unit_of_measurement="m", accuracy_decimals=3, state_class="measurement"
        ),
        cv.Optional(CONF_FP_POWER): sensor.sensor_schema(
            unit_of_measurement="dBm", accuracy_decimals=1, state_class="measurement"
        ),
        cv.Optional(CONF_RX_POWER): sensor.sensor_schema(
            unit_of_measurement="dBm", accuracy_decimals=1, state_class="measurement"
        ),
        cv.Optional(CONF_CIR_RATIO): sensor.sensor_schema(
            unit_of_measurement="dB", accuracy_decimals=1, state_class="measurement"
        ),
        cv.Optional(CONF_NLOS_POWER): sensor.sensor_schema(
            unit_of_measurement="dBm", accuracy_decimals=1, state_class="measurement"
        ),
        cv.Optional(CONF_STATUS): sensor.sensor_schema(
            accuracy_decimals=0, state_class="measurement"
        ),
    }
)

async def _register_anchor_sensor(config, key, setter):
    if key not in config:
        return []
    sens = await sensor.new_sensor(config[key])
    return [cg.add(setter(sens, config[key][CONF_ANCHOR_ID]))]


async def sensor_to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    for key, setter in [
        (CONF_DISTANCE, parent.set_distance_sensor),
        (CONF_DISTANCE_FILTERED, parent.set_distance_filtered_sensor),
    ]:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(setter(sens, config[key][CONF_ANCHOR_ID]))

    scalar_mappings = [
        (CONF_X_RAW, parent.set_x_raw_sensor),
        (CONF_Y_RAW, parent.set_y_raw_sensor),
        (CONF_X_FILTERED, parent.set_x_filtered_sensor),
        (CONF_Y_FILTERED, parent.set_y_filtered_sensor),
        (CONF_FP_POWER, parent.set_fp_power_sensor),
        (CONF_RX_POWER, parent.set_rx_power_sensor),
        (CONF_CIR_RATIO, parent.set_cir_ratio_sensor),
        (CONF_NLOS_POWER, parent.set_nlos_power_sensor),
        (CONF_STATUS, parent.set_status_sensor),
    ]

    for key, setter in scalar_mappings:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(setter(sens))
