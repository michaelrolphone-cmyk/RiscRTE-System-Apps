#!/usr/bin/env python3
"""Check persistent native crown cancellation on the delivered Watch adapter."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/watch-canvas-crown-exit'
OUT.mkdir(parents=True, exist_ok=True)
for name, provenance in json.loads((ROOT / 'lib/PortableApps/SOURCES.json').read_text()).items():
    assert hashlib.sha256((ROOT / 'lib/PortableApps/include' / name).read_bytes()).hexdigest() == provenance['sha256'], name
cases = ['pending', 'alarm', 'controls', 'settings', 'controls-open', 'torch-open',
         'slider-open', 'short-capabilities', 'short-monotonic', 'missing-hook', 'legacy-back']
for sanitized in (False, True):
    for rotation in (0, 180):
        binary = OUT / ('adapter-' + str(rotation) + ('-san' if sanitized else ''))
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        subprocess.run([
            os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
            '-DPORTABLE_NOVA_UI', '-DPORTABLE_TOUCH_ROTATION=' + str(rotation), *flags,
            '-I' + str(ROOT / 'lib/PortableApps/include'), '-I' + str(ROOT / 'lib/NativeApps/include'),
            str(ROOT / 'Apps/settings.c'), str(ROOT / 'test/native_apps/watch_canvas_crown_exit_test.c'),
            *[str(ROOT / 'lib/PortableApps/src' / name) for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c')],
            '-o', str(binary),
        ], check=True, timeout=120)
        for case in cases:
            subprocess.run([str(binary), case], check=True, timeout=20)
print('Watch canvas crown exit: 44 real-adapter cases passed in normal/ASan/UBSan modes')
