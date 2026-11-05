#!/usr/bin/env python3
"""Qualify independent Touchpad owner7 and production direct/snapshot wiring."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, default=ROOT / 'build/ble-touchpad-native-input')
a = p.parse_args()
out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
cc = os.environ.get('CC', 'cc')
includes = ['-I' + str(ROOT / p) for p in ('lib/PortableApps/include', 'lib/PortableApps/src', 'lib/NativeApps/include')]
common = [cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *includes]
selection = ['-D' + f for f in ('PORTABLE_BLE_TOUCHPAD_NATIVE_UI', 'PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_RETAINED_RGB565_HANDOFF')]
rows = []
for sanitized in (False, True):
    san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    for name, flags, source in (
        ('reducer', [], 'ble_touchpad_native_input_test.c'),
        ('direct', selection, 'ble_touchpad_native_adapter_test.c'),
        ('snapshot', selection + ['-DPORTABLE_RASTER_SNAPSHOT'], 'ble_touchpad_native_adapter_test.c'),
    ):
        label = name + ('-sanitized' if sanitized else '-normal')
        binary = out / label
        subprocess.run([*common, *san, *flags, '-Wno-unused-function', str(ROOT / 'test/native_apps' / source), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=30,
            env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (out / (label + '.log')).write_text(result.stdout + result.stderr)
        print(label + ': ' + result.stdout + result.stderr, end='', flush=True); result.check_returncode()
        rows.append({'name': label, 'passed': True, 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
# Native opt-in must leave actual paper pixels and event cadence unchanged.
fixture_text = (ROOT / 'test/native_apps/portable_handoff_test.c').read_text()
for before, after in {
    '#include "../../lib/PortableApps/src/adapter.c"': '#include ' + json.dumps(str(ROOT / 'lib/PortableApps/src/adapter.c')),
    'mock_pixels[240*243]': '*mock_pixels', 'sizeof(mock_pixels)': 'geometry_size',
    '.width=240,.height=240': '.width=geometry_width,.height=geometry_height',
    '.stride_bytes=486,': '.stride_bytes=geometry_stride,',
    '.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)': '.supported_formats=RISC_DISPLAY_FORMAT_BIT(test_format)',
    'static bool mock_snapshot(void*c,risc_touch_snapshot_v1*out){': 'static bool mock_snapshot(void*c,risc_touch_snapshot_v1*out){++paper_snapshot_reads;',
    'static int32_t mock_next(void *c,uint64_t s,risc_touch_event_v1 *e){': 'static int32_t mock_next(void *c,uint64_t s,risc_touch_event_v1 *e){++paper_event_reads;',
}.items():
    assert before in fixture_text, before
    fixture_text = fixture_text.replace(before, after)
fixture = out / 'paper-fixture.c'; fixture.write_text(fixture_text)
paper_oracle = None
for sanitized in (False, True):
    san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    for snapshot in (False, True):
        for enabled in (False, True):
            label = f'paper-on{int(enabled)}-snapshot{int(snapshot)}-san{int(sanitized)}'
            binary = out / label
            flags = [f for f in selection if enabled or f != '-DPORTABLE_BLE_TOUCHPAD_NATIVE_UI']
            if snapshot: flags += ['-DPORTABLE_RASTER_SNAPSHOT']
            subprocess.run([*common, *san, *flags, '-Wno-unused-function', '-DFRAME_FIXTURE=' + json.dumps(str(fixture)),
                str(ROOT / 'test/native_apps/ble_touchpad_native_paper_test.c'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=30,
                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
            (out / (label + '.log')).write_text(result.stdout + result.stderr)
            print(label + ': ' + result.stdout + result.stderr, end='', flush=True); result.check_returncode()
            if paper_oracle is None: paper_oracle = result.stdout
            assert result.stdout == paper_oracle, (label, result.stdout, paper_oracle)
            rows.append({'name': label, 'passed': True, 'paper_signature': result.stdout.strip()})

# Check real adapter objects without relying on the fixture's include visibility.
for snapshot in (False, True):
    obj = out / ('adapter-snapshot.o' if snapshot else 'adapter-direct.o')
    flags = selection + (['-DPORTABLE_RASTER_SNAPSHOT'] if snapshot else [])
    subprocess.run([*common, '-Wno-unused-function', *flags, '-c', str(ROOT / 'lib/PortableApps/src/adapter.c'), '-o', str(obj)], check=True)
    symbols = subprocess.check_output(['readelf', '--wide', '--symbols', str(obj)], text=True)
    for name in ('begin', 'layout', 'present', 'ready'):
        matches = [line for line in symbols.splitlines() if line.split()[-1:] == ['portable_ble_touchpad_native_' + name]]
        assert len(matches) == 1 and 'GLOBAL HIDDEN' in matches[0], (name, matches)
    rows.append({'name': obj.name, 'hidden_api_symbols': True, 'passed': True})
paths = ['lib/PortableApps/include/PortableBleTouchpadNative.h', 'lib/PortableApps/src/ble_touchpad_native_input.inc',
    'lib/PortableApps/src/adapter.c', 'lib/PortableApps/src/raster_snapshot_state.inc',
    'test/native_apps/ble_touchpad_native_input_test.c', 'test/native_apps/ble_touchpad_native_adapter_test.c',
    'test/native_apps/ble_touchpad_native_paper_test.c',
    'test/native_apps/portable_handoff_test.c', 'scripts/test_ble_touchpad_native_input.py']
(out / 'results.json').write_text(json.dumps({'rows': rows, 'hardware': False,
    'limits': 'Deterministic provider doubles; no BLE/radio, device or hardware qualification. Failed outstanding display custody deliberately remains retained; leak detection disabled (sandbox ptrace incompatible). Clean paths assert all mock grants, frames, subscriptions and tracked allocations released.',
    'source_sha256': {n: hashlib.sha256((ROOT / n).read_bytes()).hexdigest() for n in paths}}, indent=2) + '\n')
print('PASS: 14 normal/sanitized executions, exact paper signatures and both hidden-symbol adapter objects')
