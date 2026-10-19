#!/usr/bin/env python3
"""Points-off parity, with two explicit pre-existing LoRa snapshot repairs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'b98875eb0226eb209b4ec572343bcd68d5fcf373'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, default=ROOT / 'build/points-native-composition')
p.add_argument('--baseline-dir', type=Path, help='Offline directory containing the exact pinned adapter.c and raster_snapshot_state.inc')
a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
base = out / 'baseline'; base.mkdir(exist_ok=True)
for name, blob in (('adapter.c', '1597aff5e02c1976cefad5690e6bdb594d5a5645'), ('raster_snapshot_state.inc', '9c055e7db99b5d7101a6876e66c866d0eea840ad')):
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
profiles += [(name, ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', *selectors]) for name, selectors in (
    ('files', ['PORTABLE_FILES_NATIVE_UI']),
    ('frequency', ['PORTABLE_FREQUENCY_NATIVE_UI']),
    ('lora', ['PORTABLE_LORA_NATIVE_UI']),
    ('files-frequency', ['PORTABLE_FILES_NATIVE_UI', 'PORTABLE_FREQUENCY_NATIVE_UI']),
    ('files-lora', ['PORTABLE_FILES_NATIVE_UI', 'PORTABLE_LORA_NATIVE_UI']),
    ('frequency-lora', ['PORTABLE_FREQUENCY_NATIVE_UI', 'PORTABLE_LORA_NATIVE_UI']),
    ('files-frequency-lora', ['PORTABLE_FILES_NATIVE_UI', 'PORTABLE_FREQUENCY_NATIVE_UI', 'PORTABLE_LORA_NATIVE_UI']))]
for name in ('Files', 'Frequency', 'LoRa'):
    for path in (f'lib/PortableApps/include/Portable{name}Native.h',
                 f'lib/PortableApps/src/{name.lower()}_native_input.inc'):
        assert (ROOT / path).read_bytes() == subprocess.check_output(['git', 'show', BASELINE + ':' + path], cwd=ROOT), path
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
            corrected = profile == 'lora' and snapshot
            row = {'profile': profile, 'quick': quick, 'snapshot': snapshot,
                'identical': tokens[0] == tokens[1], 'corrected_baseline': corrected,
                'token_sha256': hashlib.sha256(tokens[0].encode()).hexdigest(),
                'candidate_token_sha256': hashlib.sha256(tokens[1].encode()).hexdigest()}
            if corrected:
                # b988's LoRa-only snapshot profile does not compile. Permit
                # only its missing owner declaration; every other token must
                # remain exact, and the old failure/new success are required.
                declaration = 'static\nunsigned\nraster_native_present_owner\n;'
                assert tokens[0].count(declaration) == 0
                assert tokens[1].count(declaration) == 1
                assert tokens[0] == tokens[1].replace(declaration + '\n', '', 1)
                row.update(declaration_added='static unsigned raster_native_present_owner;',
                    identical_except_missing_declaration=True)
                for label, source in (('baseline', base / 'adapter.c'),
                                      ('candidate', ROOT / 'lib/PortableApps/src/adapter.c')):
                    cmd = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-fsyntax-only',
                        *('-D' + f for f in flags),
                        *('-I' + str(ROOT / inc) for inc in ('lib/PortableApps/include', 'lib/NativeApps/include', 'lib/PortableApps/src')), str(source)]
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    (out / f'lora-snapshot-quick{int(quick)}-{label}.log').write_text(result.stdout + result.stderr)
                    row[label + '_compile_exit'] = result.returncode
                    if label == 'baseline':
                        assert result.returncode != 0 and 'raster_native_present_owner' in result.stderr and 'undeclared' in result.stderr
                    else:
                        assert result.returncode == 0, result.stderr
            else:
                assert tokens[0] == tokens[1], (profile, quick, snapshot)
            assert 'portable_points_native_' not in tokens[1]
            rows.append(row)
assert sum(row['identical'] for row in rows) == 66
assert sum(row['corrected_baseline'] for row in rows) == 2
(out / 'results.json').write_text(json.dumps({'baseline': BASELINE,
    'normalization': 'Path spelling only for 66 exact profiles; two LoRa-only snapshots permit only the missing owner declaration and require baseline compile failure / candidate success',
    'rows': rows}, indent=2) + '\n')
print('PASS: 66 exact feature-off compositions; two narrowly corrected LoRa snapshots compile; no Points API emitted')
