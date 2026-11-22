#!/usr/bin/env python3
"""Execute the production rear ranging adapter with host HTTP/Wi-Fi boundaries.

Uses the exact ArduinoJson dependency resolved by the prepared firmware build.
No device or network is accessed.
"""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    prepared = args.prepared.resolve()
    arduino_json = prepared / ".pio/libdeps/esp32cam_s3_wroom_n16r8/ArduinoJson/src"
    if not (arduino_json / "ArduinoJson.h").is_file():
        parser.error("Build the prepared firmware first to resolve its ArduinoJson dependency")
    match = re.search(r'^#define APP_VERSION "(.*?)"$',
                      (prepared / "include/settings.h").read_text(), re.M)
    if not match:
        parser.error("Prepared APP_VERSION is missing")
    compiler = shutil.which("c++")
    if not compiler:
        parser.error("A host C++ compiler is required")
    test = root / "overlay/test/ranging_disabled"
    with tempfile.TemporaryDirectory(prefix="rear-ranging-disabled-") as directory:
        binary = Path(directory) / "test-disabled"
        subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        '-DAPP_VERSION="' + match.group(1) + '"',
                        "-I", str(test / "stubs"), "-I", str(root / "overlay/include"),
                        "-I", str(arduino_json), str(test / "test_disabled.cpp"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
