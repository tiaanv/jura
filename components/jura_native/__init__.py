"""Native ESPHome Jura integration. Entity pools are finalised before API setup."""
import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor, binary_sensor, select, number, button
from esphome.const import CONF_ID, CONF_NAME, ENTITY_CATEGORY_DIAGNOSTIC
from .profiles import load_profiles

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor", "select", "number", "button"]
ns = cg.esphome_ns.namespace("jura_native")
JuraNative = ns.class_("JuraNative", cg.PollingComponent, uart.UARTDevice)
ProfileSensor = ns.class_("ProfileSensor", sensor.Sensor)
ProfileBinarySensor = ns.class_("ProfileBinarySensor", binary_sensor.BinarySensor)
RecipeSelect = ns.class_("RecipeSelect", select.Select)
RecipeNumber = ns.class_("RecipeNumber", number.Number)
CommandButton = ns.class_("CommandButton", button.Button)

TEXT = {
    "status": "Jura Status", "status_detail": "Jura Status Detail",
    "detected_model": "Detected Jura Model", "model_family": "Jura Model Family", "profile": "Jura Profile",
    "session": "Jura Session", "challenge": "Jura Challenge", "active_alerts": "Jura Active Alerts",
    "current_product": "Jura Current Product", "progress_state": "Jura Progress State", "stage_unit": "Jura Stage Unit",
    "command_result": "Jura Command Result", "requested_product": "Jura Requested Product",
    "tf_raw": "Jura TF Raw", "tv_raw": "Jura TV Raw", "counters_changed": "Changed Counters",
    "tray_status": "Tray Status", "water_tank_status": "Water Tank Status",
    "strength_choices": "Jura Current Strength Choices", "temperature_choices": "Jura Current Temperature Choices",
    "grinder_choices": "Jura Current Grinder Choices",
    "espresso_probe_result": "Jura Espresso Probe Result",
    "espresso_probe_raw": "Jura Espresso Probe Raw",
    "espresso_probe_settings": "Jura Espresso Probe Settings",
    "recipe_source": "Jura Recipe Source", "recipe_status": "Jura Recipe Status",
    "saved_recipe_raw": "Jura Saved Recipe Raw",
}
NUMERIC = {
    "progress": ("Jura Progress", "%"), "stage_current": ("Jura Stage Current", ""),
    "stage_target": ("Jura Stage Target", ""), "product_code": ("Jura Product Code", ""),
    "stage_code": ("Jura Stage Code", ""), "total_products": ("Jura Total Products", "products"),
    "timeouts": ("Jura UART Timeouts", ""), "framing_errors": ("Jura UART Framing Errors", ""),
}
for parameter, label, unit in [("water", "Water", "ml"), ("milk", "Milk", "s"), ("foam", "Milk Foam", "s"),
                               ("bypass", "Bypass", "ml"), ("pause", "Milk Pause", "s")]:
    for limit in ("min", "max", "step"):
        NUMERIC[f"{parameter}_{limit}"] = (f"Jura {label} {limit.title()}", unit)
EXTRAS = {
    "single_espresso_made": ("Single Espresso Made", "cups"), "double_espresso_made": ("Double Espresso Made", "cups"),
    "grounds_level": ("Grounds Level", "pucks"), "brew_movements": ("Brew Movements Performed", "times"),
    "brews_since_cleaning": ("Brews Performed Since Cleaning", "times"),
    "brews_since_descaling": ("Brews Performed Since Descaling", "times"),
}
BINARY = {"connected": "Jura Connected", "brew_active": "Jura Brew Active", "alerts_stale": "Jura Alerts Stale",
          "recipe_loading": "Jura Recipe Loading", "recipe_modified": "Jura Recipe Modified"}
SELECTS = {"drink": (-1, "Jura Drink"), "coffee_strength": (0, "Jura Coffee Strength"),
           "grinder_ratio": (1, "Jura Grinder Ratio"), "temperature": (2, "Jura Temperature")}
NUMBERS = {"water_amount": (3, "Jura Water Amount", "ml"), "milk_amount": (4, "Jura Milk Amount", "s"),
           "milk_foam_amount": (5, "Jura Milk Foam Amount", "s"), "bypass_amount": (6, "Jura Bypass Amount", "ml"),
           "milk_pause": (7, "Jura Milk Pause", "s")}
BUTTONS = {"brew": (0, "Jura Brew"), "reset_recipe": (1, "Jura Reset Recipe"),
           "refresh": (2, "Jura Refresh"), "power_off": (3, "Turn Off"),
           "espresso_probe": (4, "Jura Read Espresso Settings")}


def response(value):
    value = cv.string_strict(value).upper()
    if value != "AUTO" and not re.fullmatch(r"(?:[0-9A-F]{2}){1,16}", value):
        raise cv.Invalid("handshake_response must be auto or 1-16 hexadecimal bytes")
    return value


def profile_hint(value):
    value = cv.string_strict(value).upper()
    if value != "AUTO" and value not in load_profiles():
        raise cv.Invalid("Unknown EF profile")
    return value


schema = {
    cv.GenerateID(): cv.declare_id(JuraNative),
    cv.Optional("allow_brew", default=False): cv.boolean,
    cv.Optional("allow_power_off", default=False): cv.boolean,
    cv.Optional("raw_extras", default=True): cv.boolean,
    cv.Optional("startup_delay", default="8s"): cv.positive_time_period_milliseconds,
    cv.Optional("handshake_response", default="auto"): response,
    cv.Optional("profile_hint", default="auto"): profile_hint,
}
for key, name in TEXT.items():
    default = {CONF_NAME: name}
    if key.startswith("espresso_probe") or key == "saved_recipe_raw":
        default["entity_category"] = ENTITY_CATEGORY_DIAGNOSTIC
    schema[cv.Optional(key, default=default)] = text_sensor.text_sensor_schema()
for key, (name, unit) in NUMERIC.items():
    schema[cv.Optional(key, default={CONF_NAME: name})] = sensor.sensor_schema(unit_of_measurement=unit, accuracy_decimals=0)
for key, name in BINARY.items():
    schema[cv.Optional(key, default={CONF_NAME: name})] = binary_sensor.binary_sensor_schema()
for key, (_, name) in SELECTS.items():
    schema[cv.Optional(key, default={CONF_NAME: name})] = select.select_schema(RecipeSelect)
for key, (_, name, unit) in NUMBERS.items():
    schema[cv.Optional(key, default={CONF_NAME: name})] = number.number_schema(RecipeNumber, unit_of_measurement=unit)
for key, (_, name) in BUTTONS.items():
    default = {CONF_NAME: name}
    if key == "espresso_probe":
        default["entity_category"] = ENTITY_CATEGORY_DIAGNOSTIC
    schema[cv.Optional(key, default=default)] = button.button_schema(CommandButton)
CONFIG_SCHEMA = cv.Schema(schema).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("60s"))
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("jura_native", baud_rate=9600, require_tx=True, require_rx=True,
                                                        data_bits=8, parity="NONE", stop_bits=1)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    for name in ("allow_brew", "allow_power_off", "raw_extras"):
        cg.add(getattr(var, "set_" + name)(config[name]))
    cg.add(var.set_startup_delay(config["startup_delay"].total_milliseconds))
    cg.add(var.set_handshake_response("" if config["handshake_response"] == "AUTO" else config["handshake_response"]))
    cg.add(var.set_profile_hint("" if config["profile_hint"] == "AUTO" else config["profile_hint"]))
    for key in TEXT:
        entity = await text_sensor.new_text_sensor(config[key])
        cg.add(var.register_text(cg.std_string(key), entity))
    for key in NUMERIC:
        entity = await sensor.new_sensor(config[key])
        cg.add(var.register_sensor(cg.std_string(key), entity))
    for key in BINARY:
        entity = await binary_sensor.new_binary_sensor(config[key])
        cg.add(var.register_binary(cg.std_string(key), entity))
    for key, (role, _) in SELECTS.items():
        entity = await select.new_select(config[key], options=["Unavailable"])
        cg.add(entity.set_parent(var, role)); cg.add(var.register_select(role, entity))
    for key, (role, _, _) in NUMBERS.items():
        entity = await number.new_number(config[key], min_value=0, max_value=2000, step=1)
        cg.add(entity.set_parent(var, role)); cg.add(var.register_number(role, entity))
    for key, (role, _) in BUTTONS.items():
        entity = await button.new_button(config[key])
        cg.add(entity.set_parent(var, role))
        cg.add(var.register_button(role, entity))

    # All entities exist at codegen time; only the detected profile's slots become visible.
    profiles = load_profiles()
    async def profile_sensor(key, name, unit="", disabled=False):
        state_class = "total_increasing" if key.startswith("product_") or key in ("single_espresso_made", "double_espresso_made") else None
        validated = sensor.sensor_schema(ProfileSensor, unit_of_measurement=unit, accuracy_decimals=0,
                                         **({"state_class": state_class} if state_class else {}))(
            {"id": f"{config[CONF_ID].id}_{key}", "name": name, "internal": True, "disabled_by_default": disabled})
        entity = await sensor.new_sensor(validated)
        cg.add(var.register_profile_sensor(cg.std_string(key), entity))

    for code in sorted({p["code"] for profile in profiles.values() for p in profile["products"]}):
        await profile_sensor(f"product_{code}", f"Jura Product {code:02X} Count", "products")
    for i in range(max(len(p["maintenance_counts"]) for p in profiles.values())):
        await profile_sensor(f"maintenance_count_{i}", f"Jura Maintenance Counter {i}", "times")
    for i in range(max(len(p["maintenance_percentages"]) for p in profiles.values())):
        await profile_sensor(f"maintenance_percent_{i}", f"Jura Maintenance Percentage {i}", "%")
    for bit in sorted({a[0] for profile in profiles.values() for a in profile["alerts"]}):
        validated = binary_sensor.binary_sensor_schema(ProfileBinarySensor)(
            {"id": f"{config[CONF_ID].id}_alert_{bit}", "name": f"Jura Alert {bit}", "internal": True, "entity_category": ENTITY_CATEGORY_DIAGNOSTIC})
        entity = await binary_sensor.new_binary_sensor(validated)
        cg.add(var.register_alert(bit, entity))
    if config["raw_extras"]:
        for key, (name, unit) in EXTRAS.items():
            await profile_sensor(key, name, unit)
        for i in range(1, 17):
            await profile_sensor(f"raw_{i}", f"Raw Counter {i}", disabled=True)
