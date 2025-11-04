DEPENDENCIES = ["uart"]      # we require UART
AUTO_LOAD = ["sensor","text_sensor"]       # make sure sensor headers get included

import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_NAME, UNIT_EMPTY, ICON_WATER, ICON_THERMOMETER
from esphome.components import uart, sensor, text_sensor

# Icons/units
ICON_CUP = "mdi:cup"
ICON_COFFEE_MAKER = "mdi:coffee-maker"
ICON_WATER_CHECK = "mdi:water-check"
ICON_TRAY = "mdi:tray"

UNIT_CUPS = "cups"

# Top-level options
CONF_MODEL = "model"
CONF_GROUNDS_CAPACITY_CFG = "grounds_capacity"

# Model enum
MODEL_UNKNOWN = "unknown"
MODEL_F6 = "f6"
MODEL_F7 = "f7"
MODEL_E8 = "e8"

MODEL_ENUM = cv.one_of(MODEL_F6, MODEL_F7, MODEL_E8, MODEL_UNKNOWN, lower=True)

# Namespace / class
jura_ns = cg.esphome_ns.namespace("jura")
Jura = jura_ns.class_("Jura", cg.PollingComponent, uart.UARTDevice)

# -------------------------
# Per-model entity manifests
# Each tuple:
#   Numeric sensors: (key, display_name, unit, icon, accuracy_decimals)
#   Text sensors:    (key, display_name, icon)
# Keys are the stable internal identifiers you’ll also use in C++.
# -------------------------
MODEL_SPECS = {
    MODEL_F6.lower(): {
        "numeric": [
            ("counter_1", "Single Espresso Made", UNIT_CUPS, ICON_CUP, 0),
            ("counter_2", "Double Espresso Made", UNIT_CUPS, ICON_CUP, 0),
            ("counter_3", "Coffee Made", UNIT_CUPS, ICON_CUP, 0),
            ("counter_4", "Double Coffee Made", UNIT_CUPS, ICON_CUP, 0),
            ("cleanings", "Cleanings Performed", UNIT_EMPTY, ICON_WATER, 0),
            ("brews", "Brews Performed", UNIT_EMPTY, ICON_WATER, 0),
            ("grounds_remaining", "Grounds Remaining Capacity", UNIT_EMPTY, ICON_WATER, 0),
        ],
        "text": [
            ("tray_status", "Tray Status", ICON_TRAY),
            ("water_tank_status", "Water Tank Status", ICON_WATER_CHECK),
            ("machine_status", "Machine Status", ICON_COFFEE_MAKER),
        ],
    },
    MODEL_F7.lower(): {
        "numeric": [
            ("counter_1", "Single Espresso Made", UNIT_CUPS, ICON_CUP, 0),
            ("counter_2", "Double Espresso Made", UNIT_CUPS, ICON_CUP, 0),
            ("cleanings", "Cleanings Performed", UNIT_EMPTY, ICON_WATER, 0),
            ("grounds_remaining", "Grounds Remaining Capacity", UNIT_EMPTY, ICON_WATER, 0),
        ],
        "text": [
            ("tray_status", "Tray Status", ICON_TRAY),
            ("machine_status", "Machine Status", ICON_COFFEE_MAKER),
        ],
    },
    MODEL_E8.lower(): {
        "numeric": [
            ("counter_1", "Espresso Shots", UNIT_CUPS, ICON_CUP, 0),
            ("counter_2", "Coffee Cups", UNIT_CUPS, ICON_CUP, 0),
            ("cleanings", "Cleanings Performed", UNIT_EMPTY, ICON_WATER, 0),
            ("brews", "Brews Performed", UNIT_EMPTY, ICON_WATER, 0),
            ("grounds_remaining", "Grounds Remaining Capacity", UNIT_EMPTY, ICON_WATER, 0),
        ],
        "text": [
            ("tray_status", "Tray Status", ICON_TRAY),
            ("water_tank_status", "Water Tank Status", ICON_WATER_CHECK),
            ("machine_status", "Machine Status", ICON_COFFEE_MAKER),
        ],
    },
    MODEL_UNKNOWN.lower(): {
        "numeric": [
            ("counter_1", "Counter 1", UNIT_EMPTY, ICON_CUP, 0),
            ("counter_2", "Counter 2", UNIT_EMPTY, ICON_CUP, 0),
            ("grounds_remaining", "Grounds Remaining Capacity", UNIT_EMPTY, ICON_WATER, 0),
        ],
        "text": [
            ("machine_status", "Machine Status", ICON_COFFEE_MAKER),
        ],
    },
}

# Helpers to create default sensor configs
def _mk_sensor_cfg(display_name, unit, icon, acc):
    cfg = {
        "name": display_name,
        "unit_of_measurement": unit,
        "accuracy_decimals": acc,
    }
    if icon:
        cfg["icon"] = icon
    return cfg

def _mk_text_sensor_cfg(display_name, icon):
    cfg = {"name": display_name}
    if icon:
        cfg["icon"] = icon
    return cfg

# Config schema: minimal & model-driven
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Jura),
    cv.Required(CONF_MODEL): MODEL_ENUM,
    cv.Optional(CONF_GROUNDS_CAPACITY_CFG, default=12): cv.int_range(min=1, max=20),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("2s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Pass model & capacity to C++
    model = config[CONF_MODEL].upper()
    cg.add(var.set_model(model))
    cg.add(var.set_grounds_capacity(config[CONF_GROUNDS_CAPACITY_CFG]))

    spec = MODEL_SPECS.get(model.lower(), MODEL_SPECS[MODEL_UNKNOWN.lower()])

    for key, disp, unit, icon, acc in spec.get("numeric", []):
        # fresh schema for each sensor
        num_schema = sensor.sensor_schema(
            unit_of_measurement=unit,
            icon=icon,
            accuracy_decimals=acc,
        ).extend(cv.Schema({cv.Required(CONF_NAME): cv.string}))
    
        valid = num_schema({"name": disp})   # callable schema => validates & auto-generates a NEW id
        s = await sensor.new_sensor(valid)
        cg.add(var.register_metric_sensor(cg.std_string(key), s))
    
    for key, disp, icon in spec.get("text", []):
        txt_schema = text_sensor.text_sensor_schema(
            icon=icon
        ).extend(cv.Schema({cv.Required(CONF_NAME): cv.string}))
    
        valid_ts = txt_schema({"name": disp})
        ts = await text_sensor.new_text_sensor(valid_ts)
        cg.add(var.register_text_sensor(cg.std_string(key), ts))







