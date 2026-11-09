#!/usr/bin/env python3
"""Materialize the pinned ESP32CAM-RTSP radio telemetry variant."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

UPSTREAM_URL = "https://github.com/rzeldent/esp32cam-rtsp.git"
UPSTREAM_COMMIT = "14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7"
DEFAULT_BOARD = "esp32cam_s3_wroom_n16r8"


def run(argv, cwd=None):
    subprocess.run(argv, cwd=cwd, check=True)


def verify_package(root):
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("upstream_commit") != UPSTREAM_COMMIT:
        raise ValueError("Package manifest has the wrong upstream revision")
    for entry in manifest["files"]:
        relative = Path(entry["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Unsafe manifest path")
        path = root / relative
        if not path.is_file() or path.is_symlink():
            raise ValueError("Missing package file: " + str(relative))
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError("Package checksum mismatch: " + str(relative))
    return manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(".radio-build/esp32cam-rtsp"),
                        help="empty destination directory (never overwrites an existing project)")
    parser.add_argument("--build", action="store_true", help="run PlatformIO after preparing")
    parser.add_argument("-e", "--environment", default=DEFAULT_BOARD,
                        help="PlatformIO board environment, default: " + DEFAULT_BOARD)
    args = parser.parse_args(argv)
    package = Path(__file__).resolve().parent
    try:
        verify_package(package)
        output = args.output.expanduser().absolute()
        if output.exists() and (not output.is_dir() or any(output.iterdir())):
            raise ValueError("Destination must be empty; choose another --output directory: " + str(output))
        if shutil.which("git") is None:
            raise ValueError("Git is required")
        if args.build and shutil.which("pio") is None:
            raise ValueError("PlatformIO is required for --build; install it before building")
        output.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "clone", "--no-checkout", UPSTREAM_URL, str(output)])
        run(["git", "checkout", "--detach", UPSTREAM_COMMIT], cwd=output)
        actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=output, text=True).strip()
        if actual != UPSTREAM_COMMIT:
            raise ValueError("Upstream checkout did not match the pinned commit")
        patch = package / "integration.patch"
        run(["git", "apply", "--check", str(patch)], cwd=output)
        run(["git", "apply", "--whitespace=nowarn", str(patch)], cwd=output)
        overlay = package / "overlay"
        for source in sorted(overlay.rglob("*")):
            if source.is_file():
                if source.is_symlink():
                    raise ValueError("Symlinks are not supported in the overlay")
                destination = output / source.relative_to(overlay)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, destination)
        print("Prepared " + str(output))
        print("Upstream " + UPSTREAM_COMMIT)
        print("Build: pio run -d " + str(output) + " -e " + args.environment)
        if args.build:
            run(["pio", "run", "-e", args.environment], cwd=output)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print("prepare: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
