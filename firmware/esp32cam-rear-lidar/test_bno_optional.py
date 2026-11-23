#!/usr/bin/env python3
"""Execute the production optional-BNO adapter and real patched SH2 boundaries.

The adapter test uses fake GPIO/RTOS/SPI/IMU boundaries and the exact prepared
ArduinoJson and SH2 type definitions. The second binary executes the real
patched SH2/SHTP code with a deterministic no-response HAL. Neither binary uses
a device or network. --asan catches JSON strings that outlive caller storage.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--asan", action="store_true")
    parser.add_argument("--unpatched-library", type=Path,
                        help="Optional exact original v1.0.6 src directory for negative controls")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    prepared = args.prepared.resolve()
    deps = prepared / ".pio/libdeps/esp32cam_s3_wroom_n16r8"
    arduino_json = deps / "ArduinoJson/src"
    candidates = list(deps.glob("*/src/SparkFun_BNO08x_Arduino_Library.cpp"))
    if not (arduino_json / "ArduinoJson.h").is_file() or len(candidates) != 1:
        parser.error("Prepare/build firmware first to resolve its exact dependencies")
    library = candidates[0].parent
    receipt_path = library.parent / "rear-bno-safety-patch.json"
    receipt = json.loads(receipt_path.read_text())
    for name, expected in receipt["sha256"].items():
        actual = hashlib.sha256((library / name).read_bytes()).hexdigest()
        if actual != expected:
            parser.error("Prepared BNO dependency does not match its patch receipt: " + name)
    cxx, cc = shutil.which("c++"), shutil.which("cc")
    if not cxx or not cc:
        parser.error("Host C and C++ compilers are required")
    test = root / "overlay/test/bno_optional"
    with tempfile.TemporaryDirectory(prefix="rear-bno-optional-") as directory:
        directory = Path(directory)
        adapter = directory / "test-optional"
        flags = ["-fsanitize=address", "-fno-omit-frame-pointer", "-g"] if args.asan else []
        subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror", *flags,
                        "-I", str(test / "stubs"), "-I", str(root / "overlay/include"),
                        "-I", str(arduino_json), "-I", str(library),
                        str(test / "test_optional.cpp"), "-o", str(adapter)], check=True)
        import os
        env = os.environ.copy()
        if args.asan:
            env["ASAN_OPTIONS"] = "detect_stack_use_after_return=1:halt_on_error=1"
        subprocess.run([str(adapter)], check=True, timeout=15, env=env)

        def compile_sh2(source, output):
            subprocess.run([cc, "-std=c99", "-D_POSIX_C_SOURCE=200809L",
                            "-Wall", "-Wextra", "-Wno-unused-parameter",
                            "-I", str(source), str(test / "test_sh2_deadlines.c"),
                            str(source / "shtp.c"), str(source / "sh2_util.c"),
                            "-o", str(output)], check=True)

        sh2 = directory / "test-sh2"
        compile_sh2(library, sh2)
        subprocess.run([str(sh2)], check=True, timeout=5)
        if args.unpatched_library:
            original = args.unpatched_library.resolve()
            for name, expected in receipt["original_git_blobs"].items():
                data = (original / name).read_bytes()
                actual = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
                if actual != expected:
                    parser.error("Negative control is not exact original upstream: " + name)
            control = directory / "test-sh2-original"
            compile_sh2(original, control)
            for scenario in ("no-reply", "write-stall"):
                try:
                    subprocess.run([str(control), scenario], check=True, timeout=2)
                except subprocess.TimeoutExpired:
                    print("PASS negative control: original SH2 remains blocked in " + scenario,
                          flush=True)
                else:
                    raise AssertionError("Original SH2 unexpectedly finished " + scenario)


if __name__ == "__main__":
    main()
