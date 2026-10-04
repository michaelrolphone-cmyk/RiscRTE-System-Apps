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

# Combined preferences run through real root-row touch routing in one invocation.
for profile, definitions in [("raw", []), ("denver180", ["-DPORTABLE_RTC_UTC8_DENVER", "-DPORTABLE_TOUCH_ROTATION=180"])]:
    for sanitizer in (False, True):
        binary = OUT / ("settings-combined-" + profile + ("-san" if sanitizer else ""))
        flags = definitions + (["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"] if sanitizer else [])
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
            "-I" + str(ROOT / "lib/PortableApps/include"), "-I" + str(ROOT / "lib/NativeApps/include"),
            str(ROOT / "Apps/settings.c"), str(ROOT / "test/native_apps/portable_combined_settings_test.c"),
            "-o", str(binary),
        ], check=True, timeout=60)
        for scenario in range(9):
            subprocess.run([str(binary), str(scenario)], check=True, timeout=10)
print("Combined Settings: 9 scenarios x 2 display profiles x 2 compiler modes passed")

# The real shared adapter keeps both nested selectors and their private grant
# retained after the native barrier; even a direct fini call cannot release it.
for sanitizer in (False, True):
    binary = OUT / ("settings-combined-retained" + ("-san" if sanitizer else ""))
    flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"] if sanitizer else []
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
        "-I" + str(ROOT / "lib/PortableApps/include"), "-I" + str(ROOT / "lib/NativeApps/include"),
        str(ROOT / "Apps/settings.c"), str(ROOT / "test/native_apps/portable_combined_retained_settings_test.c"),
        "-o", str(binary),
    ], check=True, timeout=60)
    for scenario in range(2):
        subprocess.run([str(binary), str(scenario)], check=True, timeout=10)
