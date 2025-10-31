DEPENDENCIES = ["uart"]      # we require UART
AUTO_LOAD = ["sensor"]       # make sure sensor headers get included

import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, UNIT_PERCENT, UNIT_CELSIUS, ICON_WATER_PERCENT, ICON_THERMOMETER
from esphome.components import uart, sensor

CONF_LEVEL = "level"
CONF_TEMPERATURE = "temperature"


jura_coolcontrol_ns = cg.esphome_ns.namespace("jura_coolcontrol")
JuraCoolcontrol = jura_coolcontrol_ns.class_("JuraCoolcontrol", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(JuraCoolcontrol),
    cv.Optional(CONF_LEVEL): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        icon=ICON_WATER_PERCENT,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=1,
    ),    
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("2s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_LEVEL in config:
            lvl = await sensor.new_sensor(config[CONF_LEVEL])
            cg.add(var.set_level_sensor(lvl))

    if CONF_TEMPERATURE in config:
        tmp = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(tmp))        

