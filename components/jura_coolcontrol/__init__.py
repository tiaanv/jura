import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, binary_sensor, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]
ns = cg.esphome_ns.namespace("jura_coolcontrol")
JuraCoolcontrol = ns.class_("JuraCoolcontrol", cg.PollingComponent, uart.UARTDevice)
NONZERO_TIME = cv.All(cv.positive_not_null_time_period, cv.positive_time_period_milliseconds)
PROBE_SCHEMA = cv.Schema({
    cv.Optional("text", default="\r\n"): cv.All(cv.string_strict, cv.Length(min=1, max=128)),
    cv.Optional("delay", default="1s"): cv.positive_time_period_milliseconds,
    cv.Optional("retry_interval", default="2s"): NONZERO_TIME,
    cv.Optional("max_attempts", default=1): cv.int_range(min=1, max=5),
})
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(JuraCoolcontrol),
    cv.Optional("level", default={"name": "Milk Level"}): sensor.sensor_schema(unit_of_measurement="%", accuracy_decimals=0),
    cv.Optional("temperature", default={"name": "Milk Temperature"}): sensor.sensor_schema(unit_of_measurement="°C", accuracy_decimals=1),
    cv.Optional("raw_message"): text_sensor.text_sensor_schema(),
    cv.Optional("bridge_packet"): sensor.sensor_schema().extend({cv.Optional("internal", default=True): cv.boolean}),
    cv.Optional("message_counter"): sensor.sensor_schema(accuracy_decimals=0, icon="mdi:counter").extend({
        cv.Optional("force_update", default=True): cv.boolean,
    }),
    cv.Optional("status"): text_sensor.text_sensor_schema(),
    cv.Optional("online"): binary_sensor.binary_sensor_schema(device_class="connectivity"),
    cv.Optional("communication_timeout", default="10s"): NONZERO_TIME,
    cv.Optional("measurement_timeout", default="10s"): NONZERO_TIME,
    cv.Optional("startup_probe"): PROBE_SCHEMA,
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("2s"))

def _validate_uart(config):
    return uart.final_validate_device_schema(
        "jura_coolcontrol", baud_rate=9600, require_rx=True,
        require_tx="startup_probe" in config, data_bits=8, parity="NONE", stop_bits=1,
    )(config)

FINAL_VALIDATE_SCHEMA = _validate_uart

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_level_sensor(await sensor.new_sensor(config["level"])))
    cg.add(var.set_temperature_sensor(await sensor.new_sensor(config["temperature"])))
    if "raw_message" in config:
        cg.add(var.set_raw_message_sensor(await text_sensor.new_text_sensor(config["raw_message"])))
    if "bridge_packet" in config:
        cg.add(var.set_bridge_packet_sensor(await sensor.new_sensor(config["bridge_packet"])))
    if "message_counter" in config:
        cg.add(var.set_message_counter_sensor(await sensor.new_sensor(config["message_counter"])))
    if "status" in config:
        cg.add(var.set_status_sensor(await text_sensor.new_text_sensor(config["status"])))
    if "online" in config:
        cg.add(var.set_online_sensor(await binary_sensor.new_binary_sensor(config["online"])))
    cg.add(var.set_communication_timeout(config["communication_timeout"]))
    cg.add(var.set_measurement_timeout(config["measurement_timeout"]))
    if "startup_probe" in config:
        probe = config["startup_probe"]
        cg.add(var.set_startup_probe(probe["text"], probe["delay"], probe["retry_interval"], probe["max_attempts"]))
