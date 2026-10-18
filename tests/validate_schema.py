"""Run with Python and ESPHome 2026.9.1 installed."""
import copy
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
import yaml

base = yaml.safe_load((root / "examples/e8-local.yaml").read_text())
base["external_components"][0]["source"]["path"] = str(root / "components")
variants = [("esp32-c3-idf", base, True)]
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
monitor = copy.deepcopy(base)
monitor["jura_native"] = {"uart_id": "uart_bus", "id": "jura_coffee", "profile_hint": "EF533"}
variants.append(("monitor-defaults-profile-hint", monitor, True))
cool = copy.deepcopy(base)
cool["external_components"][0]["components"].append("jura_coolcontrol")
cool["uart"] = [cool["uart"], {"id": "cool_uart", "rx_pin": "GPIO5", "baud_rate": 9600}]
cool["jura_coolcontrol"] = {"uart_id": "cool_uart"}
variants.append(("separate-cool-control", cool, True))
for name, section, key, value in [
    ("bad-baud", "uart", "baud_rate", 19200),
    ("bad-response", "jura_native", "handshake_response", "81G"),
    ("bad-profile", "jura_native", "profile_hint", "EF999999"),
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
