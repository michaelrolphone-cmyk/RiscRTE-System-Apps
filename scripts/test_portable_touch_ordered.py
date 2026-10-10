#!/usr/bin/env python3
"""Ordered PortableTouch qualification, optionally with actual GT911 0.1.9.

All generated files stay in --output-dir (default: a new /tmp directory).
Use --product for the X4 product checkout and --sdk for a staged canonical SDK.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GT911_SHA256 = "da287cbd675b6a59131b97278a48c6cf1734b90e30a9e2674e72b0063c9a86d1"
SCENARIOS = ["bursts", "same-tick-recontacts", "same-tick-home", "same-tick-burst",
             "subscribers", "inherited", "full-queue", "multi-home", "identity", "identity-home", "move-return",
             "reset", "overflow", "read-failure", "ack-failure", "partial-read",
             "malformed-report", "recontact-recovery", "terminal-next", "terminal-snapshot",
             "terminal-move-snapshot", "terminal-unlock"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, **kwargs):
    subprocess.run(list(map(str, command)), check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--product", type=Path)
    parser.add_argument("--sdk", type=Path)
    args = parser.parse_args()
    if bool(args.product) != bool(args.sdk):
        parser.error("--product and --sdk are required together")
    out = args.output_dir.resolve() if args.output_dir else Path(tempfile.mkdtemp(prefix="portable-touch-ordered-"))
    out.mkdir(parents=True, exist_ok=True)
    include = ROOT / "lib/PortableApps/include"
    unit = ROOT / "test/native_apps/portable_touch_ordered_test.c"
    actual = ROOT / "test/native_apps/portable_touch_ordered_gt911_test.c"
    sources = [include / "PortableTouch.h", unit, actual, Path(__file__).resolve()]
    driver = fixture = None
    sdk = out / "sdk"
    if args.product:
        driver = args.product.resolve() / "minimal/drivers/x4pro_gt911/driver.c"
        fixture = args.product.resolve() / "minimal/test/gt911_test.c"
        manifest = driver.with_name("manifest.json")
        assert json.loads(manifest.read_text())["version"] == "0.1.9"
        assert digest(driver) == GT911_SHA256, "Expected exact selected production GT911 0.1.9 source"
        sdk.mkdir(exist_ok=True)
        # Canonical headers are staged once; the consumer and actual driver then
        # include the same ABI files, avoiding duplicate local SDK definitions.
        for header in args.sdk.resolve().glob("*.h"):
            target = sdk / header.name
            if target.exists() or target.is_symlink():
                target.unlink()
            target.symlink_to(header)
        for name in ("PortableTouch.h", "PortableNativeCustody.h"):
            target = sdk / name
            if target.exists() or target.is_symlink():
                target.unlink()
            shutil.copy2(include / name, target)
        sources += [driver, fixture, manifest]
    common = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    native = ["-DPORTABLE_NATIVE_CUSTODY_FENCE", "-DPORTABLE_STAGE_LOGS", "-DPORTABLE_TOUCH_SCROLL"]
    modes = {"normal": [], "asan": ["-fsanitize=address"], "ubsan": ["-fsanitize=undefined"]}
    records = []
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    for mode, sanitizers in modes.items():
        dest = out / mode
        dest.mkdir(exist_ok=True)
        san = sanitizers + (["-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie"] if sanitizers else [])
        for profile, defines in [("portable", []), ("native", native), ("native-180", native + ["-DPORTABLE_TOUCH_ROTATION=180"])]:
            binary = dest / profile
            run([os.environ.get("CC", "cc"), *common, *san, *defines, "-I" + str(include), unit, "-o", binary])
            with (dest / (profile + ".log")).open("w") as log:
                run([binary], stdout=log, stderr=subprocess.STDOUT, env=env, timeout=20)
            records.append(dict(layer="synthetic-provider", mode=mode, profile=profile))
        if driver:
            binary = dest / "gt911"
            run([os.environ.get("CC", "cc"), *common, *san, *native, "-Wno-unused-function", "-Wno-missing-field-initializers",
                 "-DX4_GT911_FIXTURE=" + json.dumps(str(fixture)), "-I" + str(sdk), actual, driver, "-o", binary])
            for scenario in SCENARIOS:
                with (dest / ("gt911-" + scenario + ".log")).open("w") as log:
                    run([binary, scenario], stdout=log, stderr=subprocess.STDOUT, env=env, timeout=20)
                records.append(dict(layer="actual-gt911-0.1.9", mode=mode, scenario=scenario))
        print(mode + ": ordered consumer qualification passed", flush=True)
    receipt = {"schema": 1, "runs": records, "hardware_verified": False, "publication": "none",
               "driver_version": "0.1.9" if driver else None,
               "source_sha256": {str(p): digest(p) for p in sources}}
    (out / "evidence.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"{len(records)} runs passed; evidence: {out / 'evidence.json'}")


if __name__ == "__main__":
    main()
