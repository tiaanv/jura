"""Portable protocol/entity tests. Set CXX to clang++, g++ or a compiler path.

Example: CXX=clang++ python tests/run_tests.py
Requires C++17. Generated executables go in tests/build.
"""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / "tests/build"
build.mkdir(exist_ok=True)
compiler = os.environ.get("CXX", "g++")
for name, defines in [("test_capabilities", []), ("test_coolcontrol_bridge", []), ("test_coolcontrol", []), ("test_controller", []), ("test_native", []), ("test_legacy", []),
                      ("test_controller", ["-DUSE_ESP8266"])]:
    suffix = "-flash" if defines else ""
    binary = build / (name + suffix + (".exe" if os.name == "nt" else ""))
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(root / "tests"), *defines,
                    str(root / "tests" / (name + ".cpp")), "-o", str(binary)], check=True)
    if name == 'test_capabilities':
        from validate_capabilities import validate
        validate(binary)
    else:
        subprocess.run([str(binary)], check=True)
