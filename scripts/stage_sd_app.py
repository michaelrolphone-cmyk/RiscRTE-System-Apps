#!/usr/bin/env python3
"""Stage an immutable SD package for explicit App Store admission, not launch.

The output is a new Apps/<id> directory containing a digest-named image and
install.json. Copy its contents into that app's directory after USB export is
mounted. Do not overwrite manifest.json or existing images. The device validates
SHA/ELF/imports/capabilities and owns durable activation after confirmation.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path


def stage(manifest_path, image_path, output):
    manifest = json.loads(manifest_path.read_text())
    identity = manifest.get('id')
    if manifest.get('type') != 'application' or not isinstance(identity, str) or not re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]{0,94}', identity):
        raise ValueError('A valid application identity is required')
    version = manifest.get('version')
    if not isinstance(version, str) or not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version) or len(version) >= 32 or any(int(p) > 0xffffffff for p in version.split('.')):
        raise ValueError('A canonical three-part version is required')
    if not isinstance(manifest.get('requires'), list):
        raise ValueError('Explicit requirements are required')
    image = image_path.read_bytes()
    if not 0 < len(image) <= 2 * 1024 * 1024 or image[:7] != b'\x7fELF\x01\x01\x01' or len(image) < 20 or int.from_bytes(image[18:20], 'little') != 94:
        raise ValueError('A bounded little-endian Xtensa ELF is required; native admission still runs on device')
    digest = hashlib.sha256(image).hexdigest()
    name = 'image-' + digest + '.elf'
    if len(f'Apps/{identity}/{name}') >= 128:
        raise ValueError('Package identity exceeds the existing native launch-path bound')
    manifest = dict(manifest, file_name=name)
    manifest.pop('description', None)
    encoded = json.dumps(dict(manifest=manifest, size=len(image), sha256=digest), separators=(',', ':'), ensure_ascii=True).encode()
    if len(encoded) > 4096:
        raise ValueError('Staged descriptor exceeds 4096 bytes; no fields were truncated')
    if output.exists():
        raise ValueError('Output must be a new staging directory')
    folder = output / 'Apps' / identity
    folder.mkdir(parents=True)
    (folder / name).write_bytes(image)
    (folder / 'install.json').write_bytes(encoded)
    return dict(id=identity, version=version, image_sha256=digest, image_bytes=len(image), directory=str(folder), installed=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.manifest, args.image, args.output), indent=2))
