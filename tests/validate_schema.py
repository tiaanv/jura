"""Run with Python and ESPHome 2026.9.1 installed."""
import copy
import base64
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
import yaml

base = yaml.safe_load((root / "examples/e8-local.yaml").read_text())
base["external_components"][0]["source"]["path"] = str(root / "components")
variants = [("esp32-c3-idf", base, True)]
no_reads = copy.deepcopy(base)
no_reads["jura_native"]["read_saved_recipes"] = False
variants.append(("native-saved-reads-disabled", no_reads, True))
bridge = yaml.safe_load((root / "examples/e8-coolcontrol-ha.yaml").read_text())
bridge["external_components"][0]["source"] = {"type": "local", "path": str(root / "components")}
variants.append(("native-coolcontrol-homeassistant", bridge, True))
for mode in ("negotiated", "legacy"):
    mode_config = copy.deepcopy(bridge)
    mode_config["jura_native"]["coolcontrol"]["reporting"] = mode
    variants.append(("native-coolcontrol-" + mode, mode_config, True))
bad_mode = copy.deepcopy(bridge)
bad_mode["jura_native"]["coolcontrol"]["reporting"] = "force_extended"
variants.append(("native-coolcontrol-bad-reporting", bad_mode, False))
missing_bridge_sensor = copy.deepcopy(bridge)
del missing_bridge_sensor["jura_native"]["coolcontrol"]["message_counter"]
variants.append(("native-coolcontrol-missing-counter", missing_bridge_sensor, False))
bad_coalesce = copy.deepcopy(bridge)
bad_coalesce["jura_native"]["coolcontrol"]["coalesce_delay"] = "15s"
variants.append(("native-coolcontrol-invalid-coalesce", bad_coalesce, False))

for name, example in (("direct-cooler-provider", "coolcontrol-direct.yaml"),
                      ("direct-cooler-consumer", "e8-coolcontrol-direct.yaml")):
    direct = yaml.safe_load((root / "examples" / example).read_text())
    direct["external_components"][0]["source"] = {"type": "local", "path": str(root / "components")}
    variants.append((name, direct, True))
    if name == "direct-cooler-provider":
        defaults = copy.deepcopy(direct)
        del defaults["jura_coolcontrol"]["bridge_packet"]["internal"]
        variants.append(("direct-cooler-default-internal", defaults, True))
    if name == "direct-cooler-consumer":
        fast_timeout = copy.deepcopy(direct)
        fast_timeout["jura_native"]["coolcontrol"]["timeout"] = "200ms"
        variants.append(("direct-cooler-no-coalesce", fast_timeout, True))
        empty = copy.deepcopy(direct)
        del empty["jura_native"]["coolcontrol"]["packet"]
        variants.append(("direct-cooler-missing-inputs", empty, False))
        mixed = copy.deepcopy(direct)
        mixed["jura_native"]["coolcontrol"]["raw_message"] = "cooler_raw"
        variants.append(("direct-cooler-mixed-inputs", mixed, False))

short_status = copy.deepcopy(base)
short_status["jura_native"]["accept_short_status"] = True
variants.append(("native-five-byte-ef533-status", short_status, True))
an01 = copy.deepcopy(base)
an01["button"] = yaml.safe_load((root / "examples/an01-test.yaml").read_text())["button"]
variants.append(("native-an01-c3-idf", an01, True))
arduino = copy.deepcopy(base)
arduino["esp32"]["framework"]["type"] = "arduino"
variants.append(("esp32-c3-arduino", arduino, True))
esp8266 = copy.deepcopy(base)
del esp8266["esp32"]
esp8266["esp8266"] = {"board": "d1_mini"}
esp8266["logger"] = {"baud_rate": 0}
variants.append(("esp8266-arduino", esp8266, True))
legacy = copy.deepcopy(esp8266)
legacy["external_components"][0]["components"] = ["jura"]
del legacy["jura_native"]
legacy["jura"] = {"uart_id": "uart_bus", "id": "jura_coffee", "model": "E8"}
legacy["api"] = {}  # Native recipe API lambdas do not belong to the legacy component.
variants.append(("legacy-jura-esp8266", legacy, True))
legacy_c3 = yaml.safe_load((root / "examples/legacy-e8.yaml").read_text())
legacy_c3["external_components"][0]["source"] = {"type": "local", "path": str(root / "components")}
variants.append(("legacy-jura-c3-idf", legacy_c3, True))
legacy_arduino = copy.deepcopy(legacy_c3)
legacy_arduino["esp32"]["framework"]["type"] = "arduino"
variants.append(("legacy-jura-c3-arduino", legacy_arduino, True))


class ExampleLoader(yaml.SafeLoader):
    pass


def example_secret(loader, node):
    name = loader.construct_scalar(node)
    # Deterministic test data only; not credentials for a deployed device.
    return base64.b64encode(bytes(range(32))).decode() if name == "api_key" else "test-placeholder"


ExampleLoader.add_constructor("!secret", example_secret)
for model in ("e6_e8", "f6", "f7"):
    example = yaml.load((root / "legacy/examples" / (model + ".yaml")).read_text(), Loader=ExampleLoader)
    example["external_components"][0]["source"] = {"type": "local", "path": str(root / "components")}
    variants.append(("legacy-example-" + model, example, True))
monitor = copy.deepcopy(base)
monitor["jura_native"] = {"uart_id": "uart_bus", "id": "jura_coffee", "profile_hint": "EF533"}
variants.append(("monitor-defaults-profile-hint", monitor, True))
cool = copy.deepcopy(base)
cool["external_components"][0]["components"].append("jura_coolcontrol")
cool["uart"] = [cool["uart"], {"id": "cool_uart", "rx_pin": "GPIO5", "baud_rate": 9600}]
cool["jura_coolcontrol"] = {"uart_id": "cool_uart"}
variants.append(("separate-cool-control", cool, True))
probe = copy.deepcopy(cool)
probe["uart"][1]["tx_pin"] = "GPIO6"
probe["jura_coolcontrol"].update({
    "status": {"name": "Cooler Status"}, "online": {"name": "Cooler Online"},
    "raw_message": {"name": "Cooler Raw Message"}, "message_counter": {"name": "Cooler Message Counter"},
    "communication_timeout": "20s", "measurement_timeout": "10s",
    "startup_probe": {"text": "hello\r\n", "delay": "1s", "retry_interval": "2s", "max_attempts": 3},
})
variants.append(("cool-control-startup-probe", probe, True))
no_tx = copy.deepcopy(probe)
del no_tx["uart"][1]["tx_pin"]
variants.append(("cool-control-probe-needs-tx", no_tx, False))
for key, value in [("text", ""), ("max_attempts", 0), ("max_attempts", 6), ("retry_interval", "0s")]:
    invalid = copy.deepcopy(probe)
    invalid["jura_coolcontrol"]["startup_probe"][key] = value
    variants.append(("cool-control-invalid-" + key + str(value), invalid, False))

for name, section, key, value in [
    ("bad-saved-reads", "jura_native", "read_saved_recipes", "disabled"),
    ("bad-baud", "uart", "baud_rate", 19200),
    ("bad-response", "jura_native", "handshake_response", "81G"),
    ("bad-profile", "jura_native", "profile_hint", "EF999999"),
    ("bad-short-status", "jura_native", "accept_short_status", "five-bytes"),
]:
    bad = copy.deepcopy(base)
    bad[section][key] = value
    variants.append((name, bad, False))

dest = root / "tests/generated-configs"
dest.mkdir(exist_ok=True)
for name, config, expected in variants:
    file = dest / (name + ".yaml")
    file.write_text(yaml.safe_dump(config, sort_keys=False), encoding="utf-8")
    result = subprocess.run([sys.executable, "-m", "esphome", "config", str(file)],
                            capture_output=True, text=True)
    if (result.returncode == 0) != expected:
        print(result.stdout, result.stderr)
        raise SystemExit("Unexpected validation result: " + name)
    print("PASS:", name)
