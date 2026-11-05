#!/usr/bin/env python3
"""Exact feature-off token parity against the pinned pre-Frequency System main."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASELINE = '4b11af9e44bc7ec71a7a63d14d898b6141c616b4'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, default=ROOT / 'build/frequency-native-optout')
p.add_argument('--baseline-dir', type=Path, help='Offline directory containing the exact pinned adapter.c and raster_snapshot_state.inc')
a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
base = out / 'baseline'; base.mkdir(exist_ok=True)
for name, blob in (('adapter.c', '17d06737f7592b6695ec9d4d554d66050ca253a4'), ('raster_snapshot_state.inc', 'a64d0018157bd589d5386eb532789db7f58642e8')):
    data = (a.baseline_dir / name).read_bytes() if a.baseline_dir else subprocess.check_output(['git', 'show', BASELINE + ':lib/PortableApps/src/' + name], cwd=ROOT)
    assert hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == blob, name
    (base / name).write_bytes(data)
profiles = [('plain', []), ('nova', ['PORTABLE_NOVA_UI']),
    ('height', ['PORTABLE_NOVA_UI', 'PORTABLE_WATCH_NATIVE_HEIGHT']),
    ('native', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS']),
    ('chrome', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_APP_OWNS_TOUCH_CHROME']),
    ('paper', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_PAPER_PREFERENCES']),
    ('scanner', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_BLE_SCANNER_NATIVE_UI']),
    ('buttons', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_BLE_BUTTONS_NATIVE_UI']), ('touchpad', ['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_BLE_TOUCHPAD_NATIVE_UI']),
    ('calculator', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_CALCULATOR_NATIVE_UI'])]
rows = []
for profile, defines in profiles:
    for quick in (False, True):
        for snapshot in (False, True):
            flags = ['PORTABLE_ALARM_CLIENT', 'PORTABLE_APP_SLEEP_LOCAL', *defines]
            if quick: flags += ['PORTABLE_QUICK_ACTIONS']
            if snapshot: flags += ['PORTABLE_RASTER_SNAPSHOT']
            tokens = []
            for source in (base / 'adapter.c', ROOT / 'lib/PortableApps/src/adapter.c'):
                cmd = [os.environ.get('CC', 'cc'), '-E', '-P', *('-D' + f for f in flags),
                    *('-I' + str(ROOT / inc) for inc in ('lib/PortableApps/include', 'lib/NativeApps/include', 'lib/PortableApps/src')), str(source)]
                data = subprocess.check_output(cmd, text=True).replace(str(base), '$ROOT/lib/PortableApps/src').replace(str(ROOT), '$ROOT')
                tokens.append('\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''', data)))
            assert tokens[0] == tokens[1], (profile, quick, snapshot)
            assert 'portable_frequency_native_' not in tokens[1]
            rows.append({'profile': profile, 'quick': quick, 'snapshot': snapshot, 'identical': True,
                'token_sha256': hashlib.sha256(tokens[0].encode()).hexdigest()})
(out / 'results.json').write_text(json.dumps({'baseline': BASELINE, 'normalization': 'None; path spelling only', 'rows': rows}, indent=2) + '\n')
print(f'PASS: {len(rows)} feature-off adapter compositions have identical preprocessed tokens; no Frequency API emitted')
