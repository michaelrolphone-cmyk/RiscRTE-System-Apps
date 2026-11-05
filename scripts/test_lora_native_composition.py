#!/usr/bin/env python3
"""Exact LoRa-off parity after composing existing Files9 and Frequency10 owners."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'c94f367f6ca3677954b580833f222ae16775c964'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, default=ROOT / 'build/lora-native-composition')
p.add_argument('--baseline-dir', type=Path, help='Offline directory containing the exact pinned adapter.c and raster_snapshot_state.inc')
a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
base = out / 'baseline'; base.mkdir(exist_ok=True)
for name, blob in (('adapter.c', 'ca73d90b653afc9777fce86e519db7163caf19bf'), ('raster_snapshot_state.inc', '9c055e7db99b5d7101a6876e66c866d0eea840ad')):
    data = (a.baseline_dir / name).read_bytes() if a.baseline_dir else subprocess.check_output(['git', 'show', BASELINE + ':lib/PortableApps/src/' + name], cwd=ROOT)
    assert hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == blob, name
    (base / name).write_bytes(data)
profiles = [('plain', []), ('nova', ['PORTABLE_NOVA_UI']),
    ('height', ['PORTABLE_NOVA_UI', 'PORTABLE_WATCH_NATIVE_HEIGHT']),
    ('native', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS']),
    ('chrome', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_APP_OWNS_TOUCH_CHROME']),
    ('paper', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_PAPER_PREFERENCES']),
    ('scanner', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_BLE_SCANNER_NATIVE_UI']),
    ('buttons', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_BLE_BUTTONS_NATIVE_UI']), ('touchpad', ['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_BLE_TOUCHPAD_NATIVE_UI']), ('calculator', ['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_CALCULATOR_NATIVE_UI'])]
profiles += [('files', ['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_FILES_NATIVE_UI']),
    ('frequency', ['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_FREQUENCY_NATIVE_UI']),
    ('files-frequency', ['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_FILES_NATIVE_UI','PORTABLE_FREQUENCY_NATIVE_UI'])]
# Existing owner implementations remain byte-identical to composed main.
for path in ('lib/PortableApps/src/files_native_input.inc', 'lib/PortableApps/src/frequency_native_input.inc',
             'lib/PortableApps/include/PortableFilesNative.h', 'lib/PortableApps/include/PortableFrequencyNative.h'):
    assert (ROOT/path).read_bytes() == subprocess.check_output(['git','show',BASELINE+':'+path],cwd=ROOT),path
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
            assert 'portable_lora_native_' not in tokens[1]
            rows.append({'profile': profile, 'quick': quick, 'snapshot': snapshot, 'identical': True,
                'token_sha256': hashlib.sha256(tokens[0].encode()).hexdigest()})
(out / 'results.json').write_text(json.dumps({'baseline': BASELINE, 'normalization': 'None; path spelling only', 'rows': rows}, indent=2) + '\n')
print(f'PASS: {len(rows)} feature-off adapter compositions have identical preprocessed tokens; no LoRa API emitted')
