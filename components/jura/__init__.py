DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor
from esphome.const import CONF_ID, CONF_NAME, UNIT_EMPTY, ENTITY_CATEGORY_DIAGNOSTIC

# Icons/units
ICON_CUP = "mdi:cup"
ICON_COFFEE_MAKER = "mdi:coffee-maker"
ICON_WATER_CHECK = "mdi:water-check"
ICON_TRAY = "mdi:tray"
ICON_WATER = "mdi:water"
UNIT_CUPS = "cups"
UNIT_PUCKS = "pucks"
UNIT_TIMES = "times"

# Top-level options
CONF_MODEL = "model"

# Model enum
MODEL_UNKNOWN = "UNKNOWN"
MODEL_F6 = "F6"
MODEL_F7 = "F7"
MODEL_E8 = "E8"
MODEL_ENUM = cv.one_of(MODEL_F6, MODEL_F7, MODEL_E8, MODEL_UNKNOWN, upper=True)

# YAML field keys (unique per entity, used only to anchor IDs)
F_SINGLE_ESPRESSO     = "single_espresso_made"
F_DOUBLE_ESPRESSO     = "double_espresso_made"
F_COFFEE              = "coffee_made"
F_DOUBLE_COFFEE       = "double_coffee_made"
F_RISTRETTO           = "ristretto_made"
F_DOUBLE_RISTRETTO    = "double_ristretto_made"
F_CAPPUCCINO          = "cappuccino_made"
F_FLAT_WHITE          = "flat_white_made"
F_MILK                = "milk_portion_made"


F_CLEANINGS           = "cleanings_performed"
F_DESCALINGS          = "descalings_performed"
F_RINSES              = "rinses_performed"
F_BREWS               = "brews_performed"
F_BREW_MOVEMENTS      = "brews_movements_performed"
F_BREWS_SINCE_CLEANNG = "brews_since_cleaning_performed"
F_GROUNDS_LEVEL       = "grounds_level"

F_TRAY_STATUS         = "tray_status"
F_TANK_STATUS         = "water_tank_status"
F_MACHINE_STATUS      = "machine_status"

F_COUNTERS_CHANGED    = "counters_changed"
F_IC_BITS = "ic_bits"

# C++ binding
jura_ns = cg.esphome_ns.namespace("jura")
Jura = jura_ns.class_("Jura", cg.PollingComponent, uart.UARTDevice)

# ---------- CONFIG SCHEMA ----------
# Give every possible entity a default dict INCLUDING a `name`.
# This ensures validation creates a unique ID for each field path.
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Jura),
    cv.Required(CONF_MODEL): MODEL_ENUM,

    # Numeric sensors (default display names; users don't have to write these)
    cv.Optional(F_SINGLE_ESPRESSO, default={CONF_NAME: "Single Espresso Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_DOUBLE_ESPRESSO, default={CONF_NAME: "Double Espresso Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_COFFEE, default={CONF_NAME: "Coffee Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_DOUBLE_COFFEE, default={CONF_NAME: "Double Coffee Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_RISTRETTO, default={CONF_NAME: "Ristretto Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_DOUBLE_RISTRETTO, default={CONF_NAME: "Double Ristretto Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_CAPPUCCINO, default={CONF_NAME: "Cappuccino Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_FLAT_WHITE, default={CONF_NAME: "Flat White Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_MILK, default={CONF_NAME: "Milk Portions Made"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_CUPS, icon=ICON_CUP, accuracy_decimals=0),
    cv.Optional(F_CLEANINGS, default={CONF_NAME: "Cleanings Performed"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_TIMES, icon=ICON_WATER, accuracy_decimals=0),
    cv.Optional(F_DESCALINGS, default={CONF_NAME: "Descalings Performed"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_TIMES, icon=ICON_WATER, accuracy_decimals=0),
    cv.Optional(F_RINSES, default={CONF_NAME: "Rinses Performed"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_TIMES, icon=ICON_WATER, accuracy_decimals=0),
    cv.Optional(F_BREWS, default={CONF_NAME: "Brews Performed"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_TIMES, icon=ICON_WATER, accuracy_decimals=0),
    cv.Optional(F_BREWS_SINCE_CLEANING, default={CONF_NAME: "Brews Performed Since Cleaning"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_TIMES, icon=ICON_WATER, accuracy_decimals=0),
    cv.Optional(F_BREW_MOVEMENTS, default={CONF_NAME: "Brew Movements Performed"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_TIMES, icon=ICON_WATER, accuracy_decimals=0),
    cv.Optional(F_GROUNDS_LEVEL, default={CONF_NAME: "Grounds Level"}):
        sensor.sensor_schema(unit_of_measurement=UNIT_PUCKS, icon=ICON_WATER, accuracy_decimals=0),

    # Text sensors
    cv.Optional(F_TRAY_STATUS, default={CONF_NAME: "Tray Status"}):
        text_sensor.text_sensor_schema(icon=ICON_TRAY),
    cv.Optional(F_TANK_STATUS, default={CONF_NAME: "Water Tank Status"}):
        text_sensor.text_sensor_schema(icon=ICON_WATER_CHECK),
    cv.Optional(F_MACHINE_STATUS, default={CONF_NAME: "Machine Status"}):
        text_sensor.text_sensor_schema(icon=ICON_COFFEE_MAKER),
    # Debug Sensors
    cv.Optional(F_COUNTERS_CHANGED, default={CONF_NAME: "Changed Counters"}):
        text_sensor.text_sensor_schema(icon="mdi:format-list-bulleted",entity_category=ENTITY_CATEGORY_DIAGNOSTIC),    
    cv.Optional(F_IC_BITS, default={CONF_NAME: "IC Bits"}):
        text_sensor.text_sensor_schema(icon="mdi:binary",entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("2s"))

# ---------- MODEL → which fields to expose & which publish keys they map to ----------
# Each entry: (yaml_field_key, publish_key)
MODEL_MAP = {
    "F6": {
        "numeric": [
            (F_SINGLE_ESPRESSO,     "counter_1"),
            (F_DOUBLE_ESPRESSO,     "counter_2"),
            (F_COFFEE,              "counter_3"),
            (F_DOUBLE_COFFEE,       "counter_4"),
            (F_BREWS,               "counter_8"),
            (F_CLEANINGS,           "counter_9"),
            (F_GROUNDS_LEVEL,       "counter_15"),
        ],
        "text": [
            (F_TRAY_STATUS,         "tray_status"),
            (F_TANK_STATUS,         "water_tank_status"),
            (F_MACHINE_STATUS,      "machine_status"),
            (F_COUNTERS_CHANGED,    "counters_changed"),
            (F_IC_BITS,             "ic_bits"),            
        ],
    },
    "F7": {
        "numeric": [
            (F_SINGLE_ESPRESSO,     "counter_1"),
            (F_DOUBLE_ESPRESSO,     "counter_2"),
            (F_COFFEE,              "counter_3"),
            (F_DOUBLE_COFFEE,       "counter_4"),
            (F_RISTRETTO,           "counter_5"),
            (F_CAPPUCCINO,          "counter_6"),
            (F_DOUBLE_RISTRETTO,    "counter_7"),
            (F_BREW_MOVEMENTS,      "counter_8"),
            (F_CLEANINGS,           "counter_9"),
            (F_DESCALINGS,          "counter_10"),
            (F_GROUNDS_LEVEL,       "counter_15"),
        ],
        "text": [
            (F_TRAY_STATUS,         "tray_status"),
            (F_TANK_STATUS,         "water_tank_status"),
            (F_MACHINE_STATUS,      "machine_status"),
            (F_COUNTERS_CHANGED,    "counters_changed"),
            (F_IC_BITS,             "ic_bits"),            
        ],
    },
    "E8": {
        "numeric": [
            (F_SINGLE_ESPRESSO,     "counter_1"),
            (F_DOUBLE_ESPRESSO,     "counter_2"),
            (F_COFFEE,              "counter_3"),
            (F_FLAT_WHITE,          "counter_4"),
            (F_CAPPUCCINO,          "counter_5"),
            (F_RINSES,              "counter_8"),
            (F_CLEANINGS,           "counter_9"),
            (F_DESCALINGS,          "counter_10"),
            (F_BREW_MOVEMENTS,      "counter_11"),
            (F_MILK,                "counter_12"), # This one is still a bit of a mystery..  It increases on a Cappuccino but not when doing milk only?
            (F_GROUNDS_LEVEL,       "counter_15"),
            (F_BRES_SINCE_CLEANING, "counter_16"),
        ],
        "text": [
            (F_TRAY_STATUS,         "tray_status"),
            (F_TANK_STATUS,         "water_tank_status"),
            (F_MACHINE_STATUS,      "machine_status"),
            (F_COUNTERS_CHANGED,    "counters_changed"),
            (F_IC_BITS,             "ic_bits"),            
        ],
    },
    "UNKNOWN": {
        "numeric": [
            (F_SINGLE_ESPRESSO,     "counter_1"),
            (F_DOUBLE_ESPRESSO,     "counter_2"),
            (F_COFFEE,              "counter_3"),
            (F_FLAT_WHITE,          "counter_4"),
            (F_CAPPUCCINO,          "counter_5"),
            (F_RINSES,              "counter_8"),
            (F_CLEANINGS,           "counter_9"),
            (F_BREW_MOVEMENTS,      "counter_11"),
            (F_GROUNDS_LEVEL,       "counter_15"),
        ],
        "text": [
            (F_TRAY_STATUS,         "tray_status"),
            (F_TANK_STATUS,         "water_tank_status"),
            (F_MACHINE_STATUS,      "machine_status"),
            (F_COUNTERS_CHANGED,    "counters_changed"),
            (F_IC_BITS,             "ic_bits"),            
        ],
    },
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    model = config[CONF_MODEL]
    cg.add(var.set_model(model))

    spec = MODEL_MAP.get(model, MODEL_MAP["UNKNOWN"])

    # Create only the entities for this model
    for field_key, publish_key in spec.get("numeric", []):
        s = await sensor.new_sensor(config[field_key])   # config[field_key] already validated with unique ID
        cg.add(var.register_metric_sensor(cg.std_string(publish_key), s))

    for field_key, publish_key in spec.get("text", []):
        ts = await text_sensor.new_text_sensor(config[field_key])
        cg.add(var.register_text_sensor(cg.std_string(publish_key), ts))


