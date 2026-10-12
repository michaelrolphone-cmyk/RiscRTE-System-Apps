#!/usr/bin/env python3
"""Prove opt-in NOVA text bytes, metrics, pixels, and unsupported-face bounds."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / 'lib/PortableApps/settings_fonts'
OUT = ROOT / 'build/nova-text-faces-012'
OUT.mkdir(parents=True, exist_ok=True)
subprocess.run([sys.executable, str(ROOT / 'scripts/generate_nova_text_faces_012.py'), '--check'], check=True)
source = (FONTS / 'text.inc').read_text()
subset = (FONTS / 'text-faces-012.inc').read_text()
manifest = json.loads((FONTS / 'text-faces-012-source.json').read_text())
assert manifest['subset_sha256'] == hashlib.sha256(subset.encode()).hexdigest()
for face in range(3):
    for ch in range(32, 127):
        # Exact initializer comparisons include every bitmap byte, dimensions,
        # signed bearing, advance, and glyph pointer name.
        name = f'rps_{face}_{ch}'
        for pattern in (rf'^static const uint8_t {name}\[\]=\{{[^\n]*\}};',
                        rf'^\{{[^\n]*,{name}\}}'):
            original = re.findall(pattern, source, re.M)
            selected = re.findall(pattern, subset, re.M)
            assert len(original) == 1 and selected == original, name
assert len(re.findall(r'^static const uint8_t rps_', subset, re.M)) == 285
assert not re.search(r'rps_[345]_', subset)
for filename in manifest['license_files']:
    assert (FONTS / filename).is_file()

incs = ['-I' + str(ROOT / p) for p in ['lib/PortableApps/include', 'lib/NativeApps/include']]
catalog = OUT / 'catalog.c'
catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
for san in (False, True):
    results = []
    for profile in (False, True):
        name = f'pixels-{int(san)}-{int(profile)}'
        frames = OUT / name
        frames.mkdir(exist_ok=True)
        flags = ['-DPORTABLE_NOVA_UI']
        if profile:
            flags += ['-DPORTABLE_NOVA_TEXT_FACES_012']
        if san:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags, *incs]
        binary = OUT / (name + '.exe')
        subprocess.run([*common, str(ROOT / 'test/native_apps/nova_text_faces_012_test.c'),
                        str(ROOT / 'lib/PortableApps/src/adapter.c'), str(catalog), '-o', str(binary)], check=True)
        subprocess.run([str(binary), str(frames)], check=True)
        results.append([(frames / f'frame-{n:03d}.ppm').read_bytes() for n in (1, 2)])
    assert results[0] == results[1], 'Generic/profile widget pixels differ'

# Prevent this restricted table from being selected for existing full-face views.
for macro in ('PORTABLE_SETTINGS_APP', 'PORTABLE_WIFI_SETTINGS_APP', 'PORTABLE_UPDATE_APP'):
    result = subprocess.run([os.environ.get('CC', 'cc'), '-E', '-DPORTABLE_NOVA_UI',
                             '-DPORTABLE_NOVA_TEXT_FACES_012', '-D' + macro, *incs,
                             str(ROOT / 'lib/PortableApps/src/adapter.c')], capture_output=True, text=True)
    assert result.returncode and 'NOVA faces 0/1/2 profile excludes' in result.stderr, macro
print('Exact glyph data, 9,120 real-adapter pixel cases, generic/profile widgets, bounds, and ASan/UBSan passed.')
