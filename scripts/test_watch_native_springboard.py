#!/usr/bin/env python3
"""Qualify the real original honeycomb on both Watch geometries.

One native executable is invoked with runtime 240x240 and 410x502 dimensions.
The 240 output is compared pixel-for-pixel with the restored 72f435f controller
and presenter, compiled from an isolated git archive. Both variants run normally
and under ASan/UBSan, with synchronous and asynchronous provider capabilities.
Unknown timing preserves this presentation without claiming fast transition
capability; known-ineligible timing and retained-image metadata still fall back.
"""
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "build/watch-native-springboard"
BASELINE = "72f435fcee376ec62d17ece6180753385ea547d9"
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


def run(binary, width, height, provider, metadata="eligible"):
    directory = OUT / f"{binary.name}-{width}x{height}-{provider}-{metadata}"
    directory.mkdir(exist_ok=True)
    # This executor runs under ptrace, where LeakSanitizer cannot inspect
    # threads. ASan bounds/use-after-free and all UBSan checks remain enabled.
    env = {**os.environ, "ASAN_OPTIONS": os.environ.get("ASAN_OPTIONS", "") + ":detect_leaks=0"}
    result = subprocess.run([str(binary), str(width), str(height), str(directory), provider, metadata],
                            env=env, timeout=60, capture_output=True, text=True)
    print(result.stdout, end="", flush=True)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    result.check_returncode()
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(directory.glob("*.ppm"))}
    if "initial.ppm" in hashes:
        assert hashes["initial.ppm"] == hashes["reentry.ppm"], hashes
    else:
        assert set(hashes) == {"generic-grid.ppm"}, hashes
    # Keep lossless, verified captures without multiplying large raw PPMs for
    # every metadata profile. Hashes below always describe the original pixels.
    with zipfile.ZipFile(directory / "frames.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(directory.glob("*.ppm")):
            archive.write(path, path.name)
    with zipfile.ZipFile(directory / "frames.zip") as archive:
        assert hashes == {name: hashlib.sha256(archive.read(name)).hexdigest()
                          for name in archive.namelist()}
    for path in directory.glob("*.ppm"):
        path.unlink()
    results.append({"binary": binary.name, "width": width, "height": height,
                    "provider": provider, "metadata": metadata, "frames": hashes, "result": result.stdout.strip()})
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
            expected_native = {(240, 240): actual}
            for width, height in ((410, 502), (320, 400)):
                # The extra geometry rejects accidental model-specific constants.
                expected_native[width, height] = run(native, width, height, provider)
            for width, height in expected_native:
                for metadata in ("unknown-refresh", "unknown-timing", "eligible-threshold"):
                    rendered = run(native, width, height, provider, metadata)
                    assert rendered == expected_native[width, height], (metadata, rendered, expected_native[width, height])
                for metadata in ("slow-refresh", "slow-latency", "retains-image",
                                 "unknown-slow-latency", "unknown-retains-image"):
                    run(native, width, height, provider, metadata)

report = {"baseline": BASELINE, "catalog_sha256": hashlib.sha256(CATALOG.read_bytes()).hexdigest(),
          "checks": results, "240_pixel_parity": True, "same_binary_both_displays": True,
          "unknown_timing_preserves_honeycomb": True, "known_ineligible_preserves_fallback": True,
          "provider_metadata_unchanged": True, "unknown_refresh_skips_retained_transition": True,
          "leak_sanitizer": "disabled: executor ptrace restriction; resource counters and guards checked"}
(OUT / "results.json").write_text(json.dumps(report, indent=2) + "\n")
print("PASS: original 240 frames identical; unknown timing preserves full-height honeycomb; known-ineligible fallback, metadata identity, touch/lifecycle and conservative transitions passed normal + ASan/UBSan")
