"""Check that the built rear update is a complete ESP image for both application slots."""

import argparse
import csv
import hashlib
import json
from pathlib import Path


def check_image(build_dir, release, factory=False):
    image = (build_dir / ".pio/build/esp32cam_s3_wroom_n16r8/firmware.bin").read_bytes()
    version = release + ("-factory" if factory else "")
    if len(image) < 56 or image[0] != 0xE9 or version.encode() + b"\0" not in image:
        raise ValueError(f"Missing ESP application header or rear version {version}")
    # Arduino's upload image ends with its ESP image SHA-256, enabled by the
    # hash-appended flag in the extended image header.
    if image[23] != 1 or hashlib.sha256(image[:-32]).digest() != image[-32:]:
        raise ValueError("Incomplete or corrupt ESP application image")

    partition_path = build_dir / "partitions/rear-ota.csv"
    with partition_path.open() as source:
        rows = [r for r in csv.reader(source) if r and not r[0].startswith("#")]
    slots = {row[2]: (int(row[3], 0), int(row[4], 0)) for row in rows if row[1] == "app"}
    if set(slots) != {"ota_0", "ota_1"}:
        raise ValueError("The rear device requires two OTA slots")
    first, second = sorted(slots.values())
    if first[0] + first[1] > second[0] or len(image) > min(first[1], second[1]):
        raise ValueError("Update cannot be stored independently in either OTA slot")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--factory", action="store_true")
    args = parser.parse_args()
    release = json.loads(Path(__file__).with_name("manifest.json").read_text())["variant"]
    check_image(args.build, release, args.factory)
