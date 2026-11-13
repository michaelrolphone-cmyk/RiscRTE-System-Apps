#!/usr/bin/env python3
"""Assemble the verified native product bundle into a first-install release."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from install_infrastructure import verify

VERSION = "v1.11.0"
PREFIX = f"riscrte-esp32s3-infrastructure-{VERSION}"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    for name in ("bundle", "runtime", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    bundle, release = args.bundle, args.output
    verify(bundle)
    manifest = json.loads((bundle / "flash-manifest.json").read_text())
    image = bytearray(b"\xff" * manifest["flash_bytes"])
    for item in manifest["files"]:
        if "offset" in item:
            start = item["offset"]
            data = (bundle / item["path"]).read_bytes()
            image[start:start + len(data)] = data
    release.mkdir(parents=True, exist_ok=False)
    binary = release / (PREFIX + "-initial.bin")
    binary.write_bytes(image)
    shutil.copyfile(bundle / "flash-manifest.json", release / "flash-manifest.json")
    shutil.copyfile(bundle / "fixtures/catalog.json", release / "catalog.json")
    shutil.copyfile(str(bundle) + ".zip", release / (PREFIX + "-repair.zip"))
    (release / "direct-image.json").write_text(json.dumps({
        "asset": binary.name, "bytes": len(image), "sha256": sha(binary),
        "source": manifest["source"], "first_install_only": True
    }, indent=2) + "\n")
    (release / "RELEASE-NOTES.md").write_text(
        "ESP32-S3 infrastructure v1.11.0 uses the same I2C driver for the camera and runtime, discards its local identification frame and captures sensor-maximum or consumer-sized JPEGs only on request "
        "on the five-mode GOOUUU ESP32-S3-CAM firmware. Use the preserving repair bundle for "
        "enrolled v1.9.0 devices. The full initial image replaces existing device data.\n")
    subprocess.run([sys.executable, str(args.runtime / "scripts/verify_infrastructure_initial_image.py"),
                    str(release)], check=True)
    (release / "SHA256SUMS").write_text("".join(
        f"{sha(path)}  {path.name}\n" for path in sorted(release.iterdir()) if path.is_file()))


if __name__ == "__main__":
    main()
