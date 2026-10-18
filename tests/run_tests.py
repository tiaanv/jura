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
for name, defines in [("test_controller", []), ("test_native", []),
                      ("test_controller", ["-DUSE_ESP8266"])]:
    suffix = "-flash" if defines else ""
    binary = build / (name + suffix + (".exe" if os.name == "nt" else ""))
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(root / "tests"), *defines,
                    str(root / "tests" / (name + ".cpp")), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
