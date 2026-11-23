#!/usr/bin/env python3
"""Check disjoint sender wall-time accounting with production transport code.

A deterministic syscall/clock boundary injects partial writes, EAGAIN, EINTR,
wake-up latency and inherited heartbeat writes. This validates accounting and
preserved wire/pacing decisions, not hardware throughput or a Wi-Fi diagnosis.
"""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prepared", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    shared = root.parent / "esp32cam-rtsp-radio" / "overlay"
    tests = root / "overlay/test/live_send_timing"
    compiler = shutil.which("c++")
    if not compiler:
        raise SystemExit("A host C++ compiler is required")
    with tempfile.TemporaryDirectory(prefix="rear-send-timing-") as directory:
        work = Path(directory)
        (work / "src").mkdir()
        names = ("live_websocket.h", "live_write_progress.h", "live_send_timing.h")
        for name in names:
            shutil.copyfile(shared / "src" / name, work / "src" / name)
        subprocess.run(["git", "apply", "--check", str(root / "live-write-progress.patch")],
                       cwd=work, check=True)
        subprocess.run(["git", "apply", str(root / "live-write-progress.patch")],
                       cwd=work, check=True)
        source = work / "src"
        if args.prepared:
            source = args.prepared.resolve() / "src"
            for name in names:
                if (source / name).read_bytes() != (work / "src" / name).read_bytes():
                    raise AssertionError("Prepared source differs: " + name)
        binary = work / "test-send-timing"
        subprocess.run([
            compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-DLIVE_WEBSOCKET_SEND_TIMING=1",
            "-I", str(tests / "stubs"),
            "-I", str(shared / "test/live_write_progress/stubs"),
            "-I", str(source),
            str(tests / "test_timing.cpp"), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True, timeout=15)


if __name__ == "__main__":
    main()
