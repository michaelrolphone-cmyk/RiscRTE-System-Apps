#!/usr/bin/env python3
"""Measure production touch/Quick work before/after without shipped counters.

Raw deterministic providers drive the native paper adapter and the established
Watch integration fixture. GCC function-entry instrumentation observes work;
every submitted frame must remain byte-identical to the immutable baseline.
"""
import argparse
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = "ba280f3c1db89f3bf42647bfdea8c205ad55e9ce"
HELPERS = ["PortableNativeTimeSource", "PortableRealtimeClient", "PortableTimeZone",
           "PortableTimeZoneCatalog", "PortableTimeZonePreference"]
QUICK = ["quick_actions", "quick_render", "quick_session"]
FIXTURES = ["touch_dispatch_cost_test.c", "watch_quick_copy_cost_test.c"]


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/touch-cost")
    args = parser.parse_args()
    out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
    receipt = {"baseline": BASE, "hardware": "not run", "shipped_instrumentation": False, "runs": {}}
    with tempfile.TemporaryDirectory(prefix="touch-cost-") as temporary:
        stage = Path(temporary); baseline = stage / "baseline"; baseline.mkdir()
        archive = subprocess.check_output(["git", "-C", ROOT, "archive", BASE])
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(baseline, filter="data")
        for name in FIXTURES:
            shutil.copyfile(ROOT / "test/native_apps" / name, baseline / "test/native_apps" / name)
        for profile in ["paper", "watch"]:
            for sanitized in [False, True]:
                results = {}
                for label, repo in [("before", baseline), ("after", ROOT)]:
                    folder = out / f"{profile}-{int(sanitized)}-{label}"; folder.mkdir(exist_ok=True)
                    include = stage / f"sdk-{profile}-{int(sanitized)}-{label}" / "include"
                    shutil.copytree(repo / "lib/PortableApps/include", include)
                    shutil.copytree(repo / "lib/PortableApps/time", include.parent / "time")
                    flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                             "-Wno-unused-function", "-finstrument-functions"]
                    if sanitized:
                        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                                  "-fno-omit-frame-pointer", "-no-pie"]
                    if profile == "paper":
                        for name in ["RiscRuntimeV1.h", "RiscRealtimeV1.h"]:
                            shutil.copyfile(args.runtime / "sdk/app" / name, include / name)
                        flags += ["-DPORTABLE_NATIVE_TIME_TOOLBAR", "-DPORTABLE_NATIVE_CUSTODY_FENCE",
                                  "-DTEST_NATIVE_TOOLBAR_QUICK"]
                        sources = [repo / "test/native_apps" / FIXTURES[0]]
                        helpers = HELPERS + QUICK
                        link = ["-Wl,--wrap=free"]
                        cases = ["stationary", "move", "tap", "cancel", "restore-retained"]
                    else:
                        flags += ["-DPORTABLE_NOVA_UI"]
                        sources = [repo / "Apps/settings.c", repo / "test/native_apps" / FIXTURES[1]]
                        helpers = QUICK; link = []; cases = list(map(str, range(13)))
                    sources += [repo / "lib/PortableApps/src" / (name + ".c") for name in helpers]
                    binary = folder / "test"
                    run([os.environ.get("CC", "cc"), *flags, "-I" + str(include),
                         "-I" + str(repo / "lib/NativeApps/include"), *sources, *link, "-o", binary])
                    results[label] = {}
                    for case in cases:
                        frames = folder / case
                        if frames.exists():
                            shutil.rmtree(frames)
                        frames.mkdir()
                        result = run([binary, case, frames], env=environment, text=True,
                                     stdout=subprocess.PIPE, timeout=30)
                        results[label][case] = json.loads(result.stdout.splitlines()[-1])
                for case in cases:
                    a, b = results["before"][case], results["after"][case]
                    assert a["background_copies"] == b["background_copies"], (profile, case)
                    assert a["restore_fills"] == a["background_copies"]
                    assert a["restore_pixel_visits"] == (0 if case == "restore-retained" else a["background_copies"] * (384000 if profile == "paper" else 57600))
                    assert b["restore_fills"] == b["restore_pixel_visits"] == 0
                    cost_keys = {"fill_calls", "restore_fills", "restore_pixel_visits"}
                    assert {k: v for k, v in a.items() if k not in cost_keys} == {k: v for k, v in b.items() if k not in cost_keys}
                    before = out / f"{profile}-{int(sanitized)}-before" / case
                    after = out / f"{profile}-{int(sanitized)}-after" / case
                    assert {p.name for p in before.iterdir()} == {p.name for p in after.iterdir()}
                    for old in before.iterdir():
                        assert old.read_bytes() == (after / old.name).read_bytes(), (profile, case, old.name)
                receipt["runs"][f"{profile}-{'sanitized' if sanitized else 'normal'}"] = results
                print(f"{profile} sanitized={sanitized}: {len(cases)} before/after cases; identical pixels, provider calls and event outcomes", flush=True)
    (out / "cost-evidence.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("Touch cost evidence: " + str(out / "cost-evidence.json"))


if __name__ == "__main__":
    main()
