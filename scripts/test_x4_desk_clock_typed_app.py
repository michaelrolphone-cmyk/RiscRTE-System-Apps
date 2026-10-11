#!/usr/bin/env python3
"""Exercise the pinned X4 typed sleep client with the current System Clock."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = (
    "terminal", "refused", "retained", "key-retained", "touch-retained",
    "touch-refused", "sd-refused", "wifi-retained", "stage-refused",
    "alarm-due", "held",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--x4", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True,
                        help="Canonical Reader sdk/driver directory")
    args = parser.parse_args()
    x4, runtime, sdk = (p.resolve() for p in (args.x4, args.runtime, args.sdk))
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    for sanitized in (False, True):
        with tempfile.TemporaryDirectory(prefix="x4-desk-clock-") as temporary:
            build = Path(temporary)
            include = build / "include"
            shutil.copytree(ROOT / "lib/PortableApps/include", include)
            shutil.copytree(ROOT / "lib/PortableApps/time", build / "time")
            for name in ("RiscDisplayOutputV1", "RiscDisplayOutputPowerV1",
                         "RiscTouchV1", "RiscTouchPowerV1", "RiscStorageVolumeV1"):
                shutil.copyfile(sdk / (name + ".h"), include / (name + ".h"))
            flags = ["-std=c11", "-Wall", "-Wextra", "-Werror"]
            flags += ["-D" + name for name in (
                "TEST_NATIVE_LANDSCAPE", "PORTABLE_DISPLAY_ROTATION=90",
                "PORTABLE_APP_OWNS_TOUCH_CHROME", "PORTABLE_RTC_WALL_TIME",
                "PORTABLE_ALARM_CLIENT", "PORTABLE_INPUT_NAVIGATION",
                "PORTABLE_APP_SLEEP_LOCAL", "PORTABLE_CROWN_SLEEP_LOCAL",
                "PORTABLE_SLEEP_MANUAL_ONLY", "PORTABLE_DESK_CLOCK",
            )]
            if sanitized:
                flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                          "-fno-omit-frame-pointer", "-no-pie"]
            flags += ["-I" + str(path) for path in (
                include, ROOT / "lib/NativeApps/include", ROOT / "test/native_apps",
                runtime / "sdk/app", runtime / "sdk/driver",
                x4 / "minimal/drivers/x4pro_power",
            )]
            sources = [ROOT / "test/native_apps/x4_desk_clock_typed_app_test.c",
                       x4 / "minimal/apps/portable_sleep.c", ROOT / "Apps/paper_clock.c",
                       ROOT / "lib/PortableApps/src/adapter.c",
                       ROOT / "lib/PortableApps/src/desk_clock_faces.c"]
            for quick in (0, 1):
                extra = []
                if quick:
                    extra = ["-DPORTABLE_QUICK_ACTIONS", "-DPORTABLE_QUICK_RADIOS"]
                    extra += [str(ROOT / "lib/PortableApps/src" / name) for name in (
                        "quick_actions.c", "quick_render.c", "quick_session.c", "quick_radios.c",
                    )]
                exe = build / "test"
                subprocess.run([os.environ.get("CC", "cc"), *flags, *extra,
                                *map(str, sources), "-o", str(exe)], check=True)

                def run(scenario, state):
                    subprocess.run([str(exe), scenario, str(state)], check=True,
                                   env=env, timeout=20)

                for scenario in SCENARIOS:
                    run(scenario, build / f"q{quick}-{scenario}.state")
                # Retained bytes and pixels are the only state crossing processes.
                # Preserve all 32 repeats and both post-cycle classifications.
                state = build / f"q{quick}-terminal.state"
                for _ in range(32):
                    run("terminal", state)
                run("seed-retained", state)
                run("gpio", state)
    print("Real Clock/adapter/pinned X4 typed client: 45 fresh processes x "
          "2 radio profiles x normal/ASan+UBSan PASS")


if __name__ == "__main__":
    main()
