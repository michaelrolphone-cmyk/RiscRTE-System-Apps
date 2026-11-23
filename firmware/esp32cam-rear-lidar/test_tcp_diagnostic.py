#!/usr/bin/env python3
"""Behavioral tests of the production rear TCP snapshot handoff and writer.

The clock, socket tuple and lwIP mailbox/PCB boundaries are deterministic fakes.
The target build and optional --prepared guard bind the real ABI separately.
No packet, firmware, interface or service operation is performed.
"""
import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path
from check_tcp_diagnostic_sdk import SDK_FILES, framework_root, validate


def sdk_guards(framework):
    validate(framework)
    with tempfile.TemporaryDirectory(prefix="rear-tcp-sdk-guard-") as directory:
        fake = Path(directory)
        for name in SDK_FILES:
            destination = fake / "tools/sdk/esp32s3" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(framework / "tools/sdk/esp32s3" / name, destination)
        validate(fake)
        for name in ("include/lwip/lwip/src/include/lwip/tcp.h", "lib/liblwip.a"):
            file = fake / "tools/sdk/esp32s3" / name
            old = file.read_bytes()
            file.write_bytes(old + b"\nchanged\n")
            try:
                validate(fake)
            except ValueError as error:
                assert name in str(error)
            else:
                raise AssertionError("Unreviewed SDK bytes were accepted: " + name)
            file.write_bytes(old)
    try:
        validate(framework, {"build.mcu": "esp32", "build.arduino.memory_type": "qio_opi"})
    except ValueError:
        pass
    else:
        raise AssertionError("Wrong SDK target was accepted")
    print("PASS actual SDK/header/archive guards and changed-input rejection", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", type=Path)
    parser.add_argument("--framework", type=Path, default=framework_root())
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    shared = root.parent / "esp32cam-rtsp-radio" / "overlay"
    tests = root / "overlay/test/tcp_diagnostic"
    compiler = shutil.which("c++")
    if not compiler:
        raise SystemExit("A host C++ compiler is required")
    with tempfile.TemporaryDirectory(prefix="rear-tcp-diagnostic-") as directory:
        work = Path(directory)
        source = work / "src"
        source.mkdir()
        names = ("live_websocket.h", "live_write_progress.h")
        for name in names:
            shutil.copyfile(shared / "src" / name, source / name)
        shutil.copyfile(root / "overlay/src/rear_tcp_diagnostic.h", source / "rear_tcp_diagnostic.h")
        subprocess.run(["git", "apply", "--check", str(root / "live-write-progress.patch")],
                       cwd=work, check=True)
        subprocess.run(["git", "apply", str(root / "live-write-progress.patch")],
                       cwd=work, check=True)
        if args.prepared:
            actual = args.prepared.resolve() / "src"
            for name in (*names, "rear_tcp_diagnostic.h"):
                if (actual / name).read_bytes() != (source / name).read_bytes():
                    raise AssertionError("Materialized source differs: " + name)
            source = actual
            sdk_guards(args.framework)
        binary = work / "test-tcp-diagnostic"
        subprocess.run([
            compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pthread",
            "-I", str(tests / "stubs"),
            "-I", str(shared / "test/live_write_progress/stubs"),
            "-I", str(source),
            str(tests / "test_diagnostic.cpp"), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True, timeout=15)


if __name__ == "__main__":
    main()
