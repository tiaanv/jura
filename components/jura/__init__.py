DEPENDENCIES = ["uart"]      # we require UART
AUTO_LOAD = ["sensor","text_sensor"]       # make sure sensor headers get included

import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, UNIT_EMPTY, ICON_WATER, ICON_THERMOMETER
from esphome.components import uart, sensor, text_sensor

ICON_CUP = "mdi:cup"
ICON_COFFEE_MAKER = "mdi:coffee-maker"
ICON_WATER_CHECK = "mdi:water-check"
ICON_TRAY = "mdi:tray"

UNIT_CUPS = "cups"

CONF_SINGLE_ESPRESSO_MADE = "single_espresso_made"
CONF_DOUBLE_ESPRESSO_MADE = "double_espresso_made"
CONF_COFFEE_MADE = "coffee_made"
CONF_DOUBLE_COFFEE_MADE = "double_coffee_made"
CONF_CLEANINGS_PERFORMED = "cleanings_performed"
CONF_BREWS_PERFORMED = "brews_performed"

CONF_GROUNDS_CAPACITY_CFG = "grounds_capacity"
CONF_GROUNDS_REMAINING_CAPACITY = "grounds_remaining_capacity"

CONF_TRAY_STATUS = "tray_status"
CONF_WATER_TANK_STATUS = "water_tank_status"
CONF_MACHINE_STATUS = "machine_status"



jura_ns = cg.esphome_ns.namespace("jura")
Jura = jura_ns.class_("Jura", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Jura),
    cv.Optional(CONF_SINGLE_ESPRESSO_MADE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CUPS,
        icon=ICON_CUP,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_DOUBLE_ESPRESSO_MADE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CUPS,
        icon=ICON_CUP,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_COFFEE_MADE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CUPS,
        icon=ICON_CUP,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_DOUBLE_COFFEE_MADE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CUPS,
        icon=ICON_CUP,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_CLEANINGS_PERFORMED): sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_WATER,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_BREWS_PERFORMED): sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_WATER,
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_GROUNDS_REMAINING_CAPACITY): sensor.sensor_schema(
        unit_of_measurement=UNIT_EMPTY,
        icon=ICON_WATER,
        accuracy_decimals=0,
    ).extend(
    cv.Schema({
        cv.Optional(CONF_GROUNDS_CAPACITY_CFG, default=16): cv.int_range(min=1, max=500),
    })
    ),
    cv.Optional(CONF_TRAY_STATUS): text_sensor.text_sensor_schema(
        icon=ICON_TRAY,
    ),
    cv.Optional(CONF_WATER_TANK_STATUS): text_sensor.text_sensor_schema(
        icon=ICON_WATER_CHECK,
    ),
    cv.Optional(CONF_MACHINE_STATUS): text_sensor.text_sensor_schema(
        icon=ICON_COFFEE_MAKER,
    ),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("2s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_SINGLE_ESPRESSO_MADE in config:
            single_espresso_made = await sensor.new_sensor(config[CONF_SINGLE_ESPRESSO_MADE])
            cg.add(var.set_single_espresso_made_sensor(single_espresso_made))

    if CONF_DOUBLE_ESPRESSO_MADE in config:
            double_espresso_made = await sensor.new_sensor(config[CONF_DOUBLE_ESPRESSO_MADE])
            cg.add(var.set_double_espresso_made_sensor(double_espresso_made))

    if CONF_COFFEE_MADE in config:
            coffee_made = await sensor.new_sensor(config[CONF_COFFEE_MADE])
            cg.add(var.set_coffee_made_sensor(coffee_made))

    if CONF_DOUBLE_COFFEE_MADE in config:
            double_coffee_made = await sensor.new_sensor(config[CONF_DOUBLE_COFFEE_MADE])
            cg.add(var.set_double_coffee_made_sensor(double_coffee_made))

    if CONF_CLEANINGS_PERFORMED in config:
            cleanings_performed = await sensor.new_sensor(config[CONF_CLEANINGS_PERFORMED])
            cg.add(var.set_cleanings_performed_sensor(cleanings_performed))

    if CONF_BREWS_PERFORMED in config:
            brews_performed = await sensor.new_sensor(config[CONF_BREWS_PERFORMED])
            cg.add(var.set_brews_performed_sensor(brews_performed))

    if CONF_GROUNDS_REMAINING_CAPACITY in config:
            grc_cfg = dict(config[CONF_GROUNDS_REMAINING_CAPACITY])

            # Extract capacity (sub-parameter) and set it on the C++ object
            cap = grc_cfg.pop(CONF_GROUNDS_CAPACITY_CFG, 16)
            cg.add(var.set_grounds_capacity(cap))        
        
            grounds_remaining_capacity = await sensor.new_sensor(grc_cfg)
            cg.add(var.set_grounds_remaining_capacity_sensor(grounds_remaining_capacity))
    
    if CONF_TRAY_STATUS in config:
            tray_status = await text_sensor.new_text_sensor(config[CONF_TRAY_STATUS])
            cg.add(var.set_tray_status_sensor(tray_status))

    if CONF_WATER_TANK_STATUS in config:
            water_tank_status = await text_sensor.new_text_sensor(config[CONF_WATER_TANK_STATUS])
            cg.add(var.set_water_tank_status_sensor(water_tank_status))

    if CONF_MACHINE_STATUS in config:
            machine_status = await text_sensor.new_text_sensor(config[CONF_MACHINE_STATUS])
            cg.add(var.set_machine_status_sensor(machine_status))





