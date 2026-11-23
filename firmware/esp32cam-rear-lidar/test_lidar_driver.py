#!/usr/bin/env python3
"""Run actual VL53L5CX 1.0.3 IO/platform/ULD regression and unchanged controls.

Fake Wire and a wrap-capable clock replace only hardware boundaries. The same
test binary source compiles against the patched dependency and its exact
receipt-bound original sources. No sensor, device endpoint or network is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

import patch_lidar_library as patch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--asan", action="store_true")
    args = parser.parse_args()
    prepared = args.prepared.resolve()
    root = Path(__file__).resolve().parent
    library = patch.locate(prepared)
    receipt_path = library.parent / patch.RECEIPT
    if not receipt_path.is_file():
        parser.error("Run the reviewed dependency patch during preparation first")
    receipt = json.loads(receipt_path.read_text())
    patch.apply(prepared)  # Existing receipt: validation only, no writes.
    original = receipt["original_source"]
    patch.validate_original(original)
    compiler = shutil.which("c++")
    if not compiler:
        parser.error("A host C++ compiler is required")
    test = root / "overlay/test/lidar_driver"
    flags = ["-fsanitize=address", "-fno-omit-frame-pointer", "-g"] if args.asan else []
    with tempfile.TemporaryDirectory(prefix="rear-lidar-driver-") as temporary:
        temporary = Path(temporary)

        def compile_driver(source, output):
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra",
                            "-Wno-unused-parameter", *flags,
                            "-I", str(test / "stubs"), "-I", str(source),
                            str(test / "test_driver.cpp"), "-o", str(output)],
                           check=True, timeout=30)

        fixed = temporary / "patched"
        compile_driver(library, fixed)
        subprocess.run([str(fixed)], check=True, timeout=10)

        unchanged = temporary / "original"
        shutil.copytree(library, unchanged)
        for name, content in original.items():
            (unchanged / name).write_bytes(content.encode())
            if patch.git_blob((unchanged / name).read_bytes()) != patch.ORIGINAL[name]:
                raise AssertionError("Original-source negative control changed: " + name)
        control = temporary / "control"
        compile_driver(unchanged, control)
        for case in ("zero-read", "short-read", "byte-error", "poll-timeout",
                     "init-timeout", "readiness-error", "range-read-error"):
            result = subprocess.run([str(control), case], text=True,
                                    capture_output=True, timeout=5)
            if result.returncode != 1 or "FAIL " + case + ":" not in result.stdout:
                raise AssertionError("Original dependency did not reproduce " + case +
                                     "\n" + result.stdout + result.stderr)
            print(result.stdout.strip(), flush=True)
            print("PASS unchanged-source negative control: " + case, flush=True)

        # Idempotence and rejection are tested as behavior against whole inputs.
        before = {name: (library / name).read_bytes() for name in patch.ORIGINAL}
        patch.apply(prepared)
        if before != {name: (library / name).read_bytes() for name in patch.ORIGINAL}:
            raise AssertionError("Repeat patch application mutated the dependency")
        print("PASS patch validation is idempotent", flush=True)

        def fixture(name, contents, with_receipt):
            directory = temporary / name
            source = directory / ".pio/libdeps/esp32cam_s3_wroom_n16r8/SparkFun VL53L5CX Arduino Library/src"
            source.mkdir(parents=True)
            for filename, content in contents.items():
                (source / filename).write_bytes(content)
            if with_receipt:
                (source.parent / patch.RECEIPT).write_bytes(receipt_path.read_bytes())
            return directory, source

        for name, contents, with_receipt in (
                ("changed-original", {key: value.encode() for key, value in original.items()}, False),
                ("changed-patched", before.copy(), True)):
            contents["platform.cpp"] += b"\n// unreviewed drift\n"
            directory, source = fixture(name, contents, with_receipt)
            snapshot = {key: (source / key).read_bytes() for key in contents}
            try:
                patch.apply(directory)
            except ValueError:
                pass
            else:
                raise AssertionError("Patch accepted unreviewed bytes: " + name)
            if snapshot != {key: (source / key).read_bytes() for key in contents}:
                raise AssertionError("Rejected patch partially rewrote sources")
            print("PASS source guard rejects " + name + " before mutation", flush=True)

        print("PASS actual dependency hashes " +
              json.dumps({name: hashlib.sha256((library / name).read_bytes()).hexdigest()
                          for name in patch.ORIGINAL}, sort_keys=True), flush=True)


if __name__ == "__main__":
    main()
