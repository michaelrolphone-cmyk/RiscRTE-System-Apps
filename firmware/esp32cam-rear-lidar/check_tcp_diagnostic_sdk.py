#!/usr/bin/env python3
"""Bind the rear TCP diagnostic to the reviewed ESP32-S3 lwIP ABI.

Read-only SDK verification: no archive/header patch and no network setting
change. The persistent callbacks inspect private PCB fields only in tcpip_thread.
An upgraded SDK must be reviewed before this diagnostic can be rebuilt.
"""
import hashlib
import json
import os
from pathlib import Path

SDK_FILES = {
    "include/lwip/lwip/src/include/lwip/tcp.h":
        "30fe24a5176042850cd083c53b11021838c86d47bb26d550866aa3424c263d47",
    "include/lwip/lwip/src/include/lwip/tcpip.h":
        "0106c89a8ba1f1cb56019e395d5681bf5a37121885a1b3218e3bf547a1266aa4",
    "include/lwip/lwip/src/include/lwip/priv/tcp_priv.h":
        "880bed9da0bb6eedc3d7c32307b99eedc78c06259274588c0292bf593661fc20",
    "include/lwip/port/esp32/include/lwipopts.h":
        "401ba8966ec48fdb3bba7b459f959448b12639a53ee214090c3fcec665d93fa6",
    "qio_opi/include/sdkconfig.h":
        "09b7ecccbd4960d49aa9aacc57662c7982221c3f49bc500c663bf117afe78fdd",
    "lib/liblwip.a":
        "8d9f1c6ef5abcf086759e3032938ef8df2c06125c0735242a0a079d277ef593d",
}


def framework_root():
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
    packages = Path(os.environ.get("PLATFORMIO_PACKAGES_DIR", str(core / "packages")))
    return packages / "framework-arduinoespressif32"


def validate(framework, board=None):
    if board is not None:
        if (board.get("build.mcu") != "esp32s3" or
                board.get("build.arduino.memory_type") != "qio_opi"):
            raise ValueError("Rear TCP diagnostic requires the reviewed esp32s3/qio_opi ABI")
    sdk = Path(framework) / "tools/sdk/esp32s3"
    for path, expected in SDK_FILES.items():
        if hashlib.sha256((sdk / path).read_bytes()).hexdigest() != expected:
            raise ValueError("Unreviewed rear TCP diagnostic SDK bytes: " + path)
    return {"schema": 1, "arduino": "2.0.17", "sdk": "esp32s3/qio_opi",
            "lwip_commit": "a45be9e438f6cf9c54ec150581819c3b95d5af6b",
            "tcp_slow_tick_ms": 500, "sha256": SDK_FILES}


if "Import" in globals():
    Import("env")
    receipt = validate(
        Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32")),
        env.BoardConfig())
    print("Rear TCP diagnostic SDK/header/archive guards passed")
elif __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--framework", type=Path, default=framework_root())
    args = parser.parse_args()
    print(json.dumps(validate(args.framework), indent=2))
