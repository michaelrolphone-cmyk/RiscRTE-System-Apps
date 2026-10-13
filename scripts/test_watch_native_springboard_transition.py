#!/usr/bin/env python3
"""Run the existing independent transition oracle at native Watch bounds.

The oracle's buffers/rejection boundary are extended from 320 to 512 in a
build-only copy. The RGB565 reference implementation is left unchanged. This
keeps all aliasing, overflow, stride and exact-allocation rejection checks.
"""
import json
import os
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "build/watch-native-springboard"
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / "test/native_apps/portable_transition_test.c").read_text()
replacements = {
    "#define TEST_MAX_SIDE 320u": "#define TEST_MAX_SIDE 512u",
    "invalid_dst, 642, invalid_old, 642, invalid_scratch, SIZE_MAX, 321, 1":
        "invalid_dst, 1026, invalid_old, 1026, invalid_scratch, SIZE_MAX, 513, 1",
    "invalid_dst, 2, invalid_old, 2, invalid_scratch, SIZE_MAX, 1, 321":
        "invalid_dst, 2, invalid_old, 2, invalid_scratch, SIZE_MAX, 1, 513",
    "prepare_frame(320, 320, 7, 3, pattern);": "prepare_frame(410, 502, 7, 3, pattern);",
    "check_phase(320, 320, 7, 3, phases[i]);": "check_phase(410, 502, 7, 3, phases[i]);",
    "    check_reference_frames();": """    check_reference_frames();
    /* Also admit the generic allocation ceiling, beyond either real panel. */
    prepare_frame(512, 512, 7, 3, 0);
    check_phase(512, 512, 7, 3, 0);
    check_phase(512, 512, 7, 3, 128);
    check_phase(512, 512, 7, 3, 256);""",
}
for old, new in replacements.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
fixture = OUT / "native-transition-oracle.c"
fixture.write_text(source)
env = {**os.environ, "ASAN_OPTIONS": os.environ.get("ASAN_OPTIONS", "") + ":detect_leaks=0"}
reports = []
for sanitized in (False, True):
    name = "native-transition-" + ("sanitized" if sanitized else "normal")
    binary = OUT / name
    flags = (["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
              "-fno-omit-frame-pointer", "-no-pie"] if sanitized else [])
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-O1", "-Wall", "-Wextra",
                    "-Werror", "-Wpedantic", "-DPORTABLE_WATCH_NATIVE_HEIGHT", *flags,
                    "-I" + str(ROOT / "lib/PortableApps/include"), str(fixture), "-o", str(binary)],
                   check=True)
    result = subprocess.run([str(binary)], env=env, timeout=180, check=True,
                            capture_output=True, text=True)
    print(result.stdout, end="", flush=True)
    reports.append({"binary": name, "result": result.stdout.strip()})
(OUT / "transition-results.json").write_text(json.dumps(reports, indent=2) + "\n")
print("PASS: native 410x502/512x512 retained transition matches the independent RGB565 oracle")
