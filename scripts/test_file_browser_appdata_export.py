#!/usr/bin/env python3
"""Actual portable browser export cleanup regressions; host-only, offline."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/file-browser-appdata-export'
OUT.mkdir(parents=True, exist_ok=True)
provenance = json.loads((ROOT / 'lib/PortableApps/SOURCES.json').read_text())
for name in ('RiscAppDataExportV1.h', 'RiscAppDataV1.h', 'RiscStorageVolumeV1.h'):
    data = (ROOT / 'lib/PortableApps/include' / name).read_bytes()
    assert hashlib.sha256(data).hexdigest() == provenance[name]['sha256'], name
fixtures = [('portable_file_browser_appdata_export_test.c', []),
            ('portable_file_browser_export_selection_test.c', []),
            ('portable_file_browser_rgb_operations_test.c', []),
            ('portable_file_browser_rgb_operations_test.c', ['-DPORTABLE_RASTER_SNAPSHOT']),
            ('portable_file_browser_read_copy_test.c', []),
            ('portable_file_browser_read_copy_test.c', ['-DPORTABLE_TOUCH_SCROLL'])]
for fixture, profile in fixtures:
    for sanitized in (False, True):
        binary = OUT / (fixture.removesuffix('.c') + ('-snapshot' if '-DPORTABLE_RASTER_SNAPSHOT' in profile else '-scroll' if profile else '') + ('-asan' if sanitized else ''))
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                        '-Wall', '-Wextra', '-Werror', *flags, *profile,
                        '-I' + str(ROOT / 'lib/PortableApps/include'),
                        '-I' + str(ROOT / 'lib/NativeApps/include'),
                        str(ROOT / 'test/native_apps' / fixture),
                        '-o', str(binary)], check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=30)
