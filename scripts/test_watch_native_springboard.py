#!/usr/bin/env python3
"""Qualify the real original honeycomb on both Watch geometries.

One native executable is invoked with runtime 240x240 and 410x502 dimensions.
The 240 output is compared pixel-for-pixel with the original 1476260 controller
and presenter, compiled from an isolated git archive. Both variants run normally
and under ASan/UBSan, with synchronous and asynchronous provider capabilities.
"""
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tarfile
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "build/watch-native-springboard"
BASELINE = "14762601b3fa7bbeeac5ed565a6825534bf74dca"
CATALOG = ROOT / "test/native_apps/fixtures/watch_springboard_catalog_23.c"
OUT.mkdir(parents=True, exist_ok=True)
CC = os.environ.get("CC", "cc")
FLAGS = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
         "-DPORTABLE_NOVA_UI", "-DPORTABLE_FORCE_FULL_FRAMES",
         "-DPORTABLE_RETAINED_RGB565_HANDOFF", "-DPORTABLE_HANDOFF_EAGER_MS=60",
         "-DPORTABLE_ASYNC_RGB565", "-DPORTABLE_CATALOG_LIMIT=23",
         '-DPORTABLE_RETURN_APP="clock.elf"']
SAN = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
       "-fno-omit-frame-pointer", "-no-pie"]
results = []


def compile_test(source, name, sanitizer, native):
    binary = OUT / name
    command = [CC, *FLAGS, *(SAN if sanitizer else []),
               *(["-DPORTABLE_WATCH_NATIVE_HEIGHT"] if native else []),
               '-DSPRINGBOARD_SOURCE="' + str(source / "Apps/springboard.c") + '"',
               "-I" + str(source / "lib/PortableApps/include"),
               "-I" + str(source / "lib/NativeApps/include"),
               str(ROOT / "test/native_apps/watch_native_springboard_test.c"),
               str(source / "lib/PortableApps/src/adapter.c"), str(CATALOG),
               "-o", str(binary)]
    subprocess.run(command, check=True)
    return binary


def run(binary, width, height, provider):
    directory = OUT / f"{binary.name}-{width}x{height}-{provider}"
    directory.mkdir(exist_ok=True)
    # This executor runs under ptrace, where LeakSanitizer cannot inspect
    # threads. ASan bounds/use-after-free and all UBSan checks remain enabled.
    env = {**os.environ, "ASAN_OPTIONS": os.environ.get("ASAN_OPTIONS", "") + ":detect_leaks=0"}
    result = subprocess.run([str(binary), str(width), str(height), str(directory), provider],
                            env=env, timeout=60, capture_output=True, text=True)
    print(result.stdout, end="", flush=True)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    result.check_returncode()
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(directory.glob("*.ppm"))}
    assert hashes["initial.ppm"] == hashes["reentry.ppm"], hashes
    results.append({"binary": binary.name, "width": width, "height": height,
                    "provider": provider, "frames": hashes, "result": result.stdout.strip()})
    return hashes


with tempfile.TemporaryDirectory(prefix="springboard-legacy-", dir=OUT) as temp:
    legacy = pathlib.Path(temp)
    archive = legacy / "source.tar"
    with archive.open("wb") as stream:
        subprocess.run(["git", "archive", BASELINE, "Apps", "lib"],
                       cwd=ROOT, stdout=stream, check=True)
    with tarfile.open(archive) as source:
        source.extractall(legacy, filter="data")
    for sanitized in (False, True):
        suffix = "sanitized" if sanitized else "normal"
        original = compile_test(legacy, "original-" + suffix, sanitized, False)
        native = compile_test(ROOT, "native-" + suffix, sanitized, True)
        for provider in ("sync", "async"):
            expected = run(original, 240, 240, provider)
            actual = run(native, 240, 240, provider)
            assert actual == expected, (suffix, provider, expected, actual)
            run(native, 410, 502, provider)
            # A further geometry rejects accidental model-specific constants.
            run(native, 320, 400, provider)

report = {"baseline": BASELINE, "catalog_sha256": hashlib.sha256(CATALOG.read_bytes()).hexdigest(),
          "checks": results, "240_pixel_parity": True, "same_binary_both_displays": True,
          "leak_sanitizer": "disabled: executor ptrace restriction; resource counters and guards checked"}
(OUT / "results.json").write_text(json.dumps(report, indent=2) + "\n")
print("PASS: original 240 frames identical; full-height native honeycomb and touch/lifecycle passed normal + ASan/UBSan")
