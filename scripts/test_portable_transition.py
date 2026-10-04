#!/usr/bin/env python3
"""Run isolated RGB565 transition checks with host address/undefined sanitizers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="portable-transition-") as temporary:
        binary = Path(temporary) / "portable_transition_test"
        subprocess.run(
            [
                *shlex.split(os.environ.get("CC", "cc")),
                "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                "-fno-omit-frame-pointer", "-no-pie",
                "-I" + str(ROOT / "lib/PortableApps/include"),
                str(ROOT / "test/native_apps/portable_transition_test.c"),
                "-o", str(binary),
            ],
            check=True,
            timeout=60,
        )
        subprocess.run([str(binary)], check=True, timeout=180)
        adapter = Path(temporary) / "portable_handoff_test"
        subprocess.run([
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
            "-fno-omit-frame-pointer", "-no-pie",
            "-DPORTABLE_RETAINED_RGB565_HANDOFF", "-DPORTABLE_FORCE_FULL_FRAMES",
            "-I" + str(ROOT / "lib/PortableApps/include"),
            "-I" + str(ROOT / "lib/NativeApps/include"),
            str(ROOT / "test/native_apps/portable_handoff_test.c"),
            "-o", str(adapter),
        ], check=True, timeout=60)
        subprocess.run([str(adapter)], check=True, timeout=60)
        eager = Path(temporary) / "portable_eager_return_test"
        subprocess.run([
            *shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie",
            "-DPORTABLE_RETAINED_RGB565_HANDOFF", "-DPORTABLE_FORCE_FULL_FRAMES",
            "-DPORTABLE_HANDOFF_EAGER_MS=60", '-DPORTABLE_RETURN_APP="return.elf"',
            "-I" + str(ROOT / "lib/PortableApps/include"), "-I" + str(ROOT / "lib/NativeApps/include"),
            str(ROOT / "test/native_apps/portable_eager_return_test.c"), "-o", str(eager),
        ], check=True, timeout=60)
        subprocess.run([str(eager)], check=True, timeout=60)



if __name__ == "__main__":
    main()
