import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]
ns = cg.esphome_ns.namespace("jura_coolcontrol")
JuraCoolcontrol = ns.class_("JuraCoolcontrol", cg.PollingComponent, uart.UARTDevice)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(JuraCoolcontrol),
    cv.Optional("level", default={"name": "Milk Level"}): sensor.sensor_schema(unit_of_measurement="%", accuracy_decimals=0),
    cv.Optional("temperature", default={"name": "Milk Temperature"}): sensor.sensor_schema(unit_of_measurement="°C", accuracy_decimals=1),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("2s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_level_sensor(await sensor.new_sensor(config["level"])))
    cg.add(var.set_temperature_sensor(await sensor.new_sensor(config["temperature"])))
