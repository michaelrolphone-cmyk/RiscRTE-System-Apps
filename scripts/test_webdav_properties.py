#!/usr/bin/env python3
"""Bounded, offline PROPFIND XML tests in ordinary and ASan/UBSan modes."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="webdav-properties-tests-") as output:
    for sanitized in (False, True):
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if sanitized else []
        if sanitized and sys.platform != "darwin":
            flags.append("-no-pie")
        binary = Path(output) / f"properties-{sanitized}"
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-pedantic", "-g", "-O1", *flags,
            "-I" + str(ROOT / "lib/RemoteFiles"),
            str(ROOT / "lib/RemoteFiles/WebDavProperties.cpp"),
            str(ROOT / "test/native_apps/webdav_properties_test.cpp"), "-o", str(binary),
        ], check=True)
        env = os.environ.copy()
        if sanitized:
            # LeakSanitizer cannot run under traced/sandboxed executors. The
            # parser itself has no allocation; address and UB checks stay on.
            env.setdefault("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1")
            env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
        subprocess.run([str(binary)], check=True, timeout=45, env=env)
print("WebDAV property request parser: ordinary and AddressSanitizer/UndefinedBehaviorSanitizer passed")
