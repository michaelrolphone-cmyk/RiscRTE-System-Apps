#!/usr/bin/env python3
"""Run production alert Settings UI/storage with plain and ASan/UBSan builds.

No deployment files or grants are changed. Both opt-in combinations acquire a
single namespace-1 grant and never request alarm-service or output authority.
"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/portable"
OUT.mkdir(parents=True, exist_ok=True)
for shared_sleep in (False, True):
    for sanitizer in (False, True):
        binary = OUT / ("settings-alert" + ("-sleep" if shared_sleep else "") + ("-san" if sanitizer else ""))
        flags = ["-DPORTABLE_SLEEP_SETTINGS"] if shared_sleep else []
        if sanitizer:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
            "-I" + str(ROOT / "lib/PortableApps/include"), "-I" + str(ROOT / "lib/NativeApps/include"),
            str(ROOT / "Apps/settings.c"), str(ROOT / "test/native_apps/portable_alarm_settings_test.c"),
            "-o", str(binary),
        ], check=True, timeout=60)
        for scenario in range(38):
            subprocess.run([str(binary), str(scenario)], check=True, timeout=10)
print("Alert Settings: 38 scenarios x 2 feature profiles x 2 compiler modes passed")
