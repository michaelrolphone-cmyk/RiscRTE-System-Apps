#!/usr/bin/env python3
"""Exercise the actual rear radio adapter with allocation faults and HTTP routes.

The production translation unit and pinned VL53L5CX/ArduinoJson headers are used
unchanged. Fakes replace ESP/RTOS/peripheral boundaries, not initialize(), its
allocation guards, snapshots or registered status handlers. No network/device
operation occurs. The original adapter fixture is independently SHA-bound.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

import patch_lidar_library as lidar


HEADERS = {
    "SparkFun_VL53L5CX_Library.h": "62745c3ad6af1d3eaa353e77bdca2149c28514fd",
    "SparkFun_VL53L5CX_IO.h": "d5ffe8cdf51fcde613a8ec765a45391c2e43c0f2",
    "SparkFun_VL53L5CX_Library_Constants.h": "607566b159df19a24a14541349434bc0cce2614e",
    "vl53l5cx_api.h": "464e0863fdc8ed763e3971f8743ae2ac716e7455",
    "platform.h": "4cb94324f2ac2f43c9df2f80658748369aa8a278",
}
BASELINE_SHA256 = "46d924f6dd1a3cfaa293b4cada60b675f64928a3b386128d17354429be04abf8"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--source", type=Path,
                        help="Exact adapter to compile (defaults to this checkout's overlay)")
    parser.add_argument("--asan", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    prepared = args.prepared.resolve()
    source = (args.source or root / "overlay/src/radio_telemetry.cpp").resolve()
    test = root / "overlay/test/lidar_adapter"
    library = lidar.locate(prepared)
    for name, expected in HEADERS.items():
        if lidar.git_blob((library / name).read_bytes()) != expected:
            raise ValueError("Pinned VL53L5CX 1.0.3 header changed: " + name)
    json_root = prepared / ".pio/libdeps/esp32cam_s3_wroom_n16r8/ArduinoJson/src"
    if not (json_root / "ArduinoJson.h").is_file():
        parser.error("Prepared ArduinoJson headers are required")
    baseline = test / "fixtures/radio_telemetry_before.cpp"
    if sha(baseline) != BASELINE_SHA256:
        raise ValueError("Unchanged-source negative control is not the reviewed original")
    compiler = shutil.which("c++")
    if not compiler:
        parser.error("A host C++ compiler is required")
    flags = ["-fsanitize=address", "-fno-omit-frame-pointer", "-g"] if args.asan else []
    with tempfile.TemporaryDirectory(prefix="rear-lidar-adapter-") as temporary:
        temporary = Path(temporary)

        def compile_adapter(adapter, output):
            subprocess.run([
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Wno-unused-parameter",
                *flags, "-I", str(test / "stubs"),
                "-I", str(root / "overlay/test/bno_optional/stubs"),
                "-I", str(root / "overlay/include"),
                "-I", str(json_root), "-I", str(library),
                '-DREAR_ADAPTER_SOURCE="' + str(adapter) + '"',
                str(test / "test_adapter.cpp"), "-o", str(output),
            ], check=True, timeout=30)

        candidate = temporary / "candidate"
        compile_adapter(source, candidate)
        subprocess.run([str(candidate)], check=True, timeout=10)

        control = temporary / "unchanged"
        compile_adapter(baseline, control)
        expected_failures = {
            "config-retained": "missing results allocation was not retried exactly once",
            "results-retained": "successful results pointer replaced or leaked",
        }
        for case, reason in expected_failures.items():
            result = subprocess.run([str(control), case], text=True,
                                    capture_output=True, timeout=5)
            if result.returncode != 1 or ("FAIL " + case + ": " + reason) not in result.stdout:
                raise AssertionError("Unchanged adapter did not reproduce " + case +
                                     "\n" + result.stdout + result.stderr)
            print(result.stdout.strip(), flush=True)
            print("PASS exact unchanged-adapter negative control: " + case, flush=True)

        print("PASS actual adapter provenance " + json.dumps({
            "candidate_source": str(source),
            "candidate_sha256": sha(source),
            "baseline_sha256": sha(baseline),
            "real_library_header_blobs": HEADERS,
            "test_source_sha256": sha(test / "test_adapter.cpp"),
            "address_sanitizer": args.asan,
        }, sort_keys=True), flush=True)


if __name__ == "__main__":
    main()
