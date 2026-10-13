#!/usr/bin/env python3
"""Run the Springboard host lifecycle with every audited deployment feature.

Pass the archived springboard-exact-build.json. All -D feature switches and
forced profile includes are preserved, plus the shared native-height switch.
Only the target-specific navigation/sleep boundaries use host test doubles.
Alarm/Broadcast/Contexts are all deliberately unavailable while Runtime remains
healthy. This verifies the optional-absence integration, not hardware service
implementations or native sleep.
"""
import argparse
import json
import os
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "build/watch-native-springboard"
parser = argparse.ArgumentParser()
parser.add_argument("recipe", type=pathlib.Path)
args = parser.parse_args()
command = json.loads(args.recipe.read_text())["command"]
features = [part for part in command if part.startswith("-D")]
forced = [command[i + 1] for i, part in enumerate(command) if part == "-include"]
required = {"PORTABLE_ALARM_CLIENT", "PORTABLE_BLE_BROADCAST", "PORTABLE_CONTEXTS_CLIENT",
            "PORTABLE_QUICK_ACTIONS", "PORTABLE_QUICK_RADIOS", "PORTABLE_LOW_BATTERY",
            "PORTABLE_RETAINED_RGB565_HANDOFF", "PORTABLE_ASYNC_RGB565"}
assert required <= {part[2:].split("=")[0] for part in features}
assert all(pathlib.Path(path).is_file() for path in forced)
OUT.mkdir(parents=True, exist_ok=True)
env = {**os.environ, "ASAN_OPTIONS": os.environ.get("ASAN_OPTIONS", "") + ":detect_leaks=0"}
results = []
for sanitized in (False, True):
    binary = OUT / ("full-features-sanitized" if sanitized else "full-features-normal")
    san = (["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
            "-fno-omit-frame-pointer", "-no-pie"] if sanitized else [])
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    *san, *features, "-DPORTABLE_WATCH_NATIVE_HEIGHT",
                    *[entry for path in forced for entry in ("-include", path)],
                    "-I" + str(ROOT / "lib/PortableApps/include"),
                    "-I" + str(ROOT / "lib/NativeApps/include"),
                    str(ROOT / "test/native_apps/watch_native_springboard_test.c"),
                    str(ROOT / "test/native_apps/fixtures/watch_springboard_catalog_23.c"),
                    *[str(ROOT / "lib/PortableApps/src" / name) for name in
                      ("adapter.c", "quick_actions.c", "quick_session.c", "quick_render.c", "quick_radios.c")],
                    "-o", str(binary)], check=True)
    for width, height in ((240, 240), (410, 502)):
        for provider in ("sync", "async"):
            for metadata in ("eligible", "unknown-refresh", "unknown-timing", "eligible-threshold",
                             "slow-refresh", "slow-latency", "retains-image",
                             "unknown-slow-latency", "unknown-retains-image"):
                result = subprocess.run([str(binary), str(width), str(height), "-", provider, metadata],
                                        env=env, check=True, timeout=60, capture_output=True, text=True)
                print(result.stdout, end="", flush=True)
                results.append({"binary": binary.name, "width": width, "height": height,
                                "provider": provider, "metadata": metadata, "result": result.stdout.strip()})
(OUT / "feature-results.json").write_text(json.dumps({"features": features, "forced_includes": forced,
                                                       "checks": results}, indent=2) + "\n")
print("PASS: every audited Springboard feature enabled; actual honeycomb startup/input/launch/return survives qualified optional-service absence")
