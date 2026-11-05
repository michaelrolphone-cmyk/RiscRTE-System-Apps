#!/usr/bin/env python3
"""Qualify private Frequency owner10 reducer and actual direct/snapshot wiring."""
import os
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/frequency-native-input'
out.mkdir(parents=True, exist_ok=True)
common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
    *('-I' + str(root / p) for p in ('lib/PortableApps/include', 'lib/PortableApps/src', 'lib/NativeApps/include'))]
selection = ['-D' + flag for flag in ('PORTABLE_FREQUENCY_NATIVE_UI', 'PORTABLE_NOVA_UI',
    'PORTABLE_NATIVE_CANVAS', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_RETAINED_RGB565_HANDOFF')]
# Extend the existing provider fixture only in the generated Quick test copy.
fixture = (root / 'test/native_apps/portable_handoff_test.c').read_text()
fixture = fixture.replace('#include "../../lib/PortableApps/src/adapter.c"',
    '#include ' + json.dumps(str(root / 'lib/PortableApps/src/adapter.c')))
alarm = '''static int32_t frequency_alarm_status(void*c,alarm_status_v1*s){(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t frequency_alarm_step(void*c){(void)c;return ALARM_OK;}
static int32_t frequency_alarm_ack(void*c,const alarm_token_v1*t){(void)t;return frequency_alarm_step(c);}
static int32_t frequency_alarm_sleep(void*c,alarm_sleep_v1*s){(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 frequency_alarm={1,sizeof(frequency_alarm),NULL,frequency_alarm_status,frequency_alarm_step,frequency_alarm_step,frequency_alarm_ack,frequency_alarm_sleep,frequency_alarm_step};
'''
needle = 'static bool mock_acquire(const char *name'
assert needle in fixture
fixture = fixture.replace(needle, alarm + needle)
needle = 'else if(!strcmp(name,"input.touch.raw"))g->api=&mock_touch;'
assert needle in fixture
fixture = fixture.replace(needle, needle + '\n    else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))g->api=&frequency_alarm;')
fixture = fixture.replace('assert(version==1 && !instance);g->struct_size',
    'if(version!=1||instance){return false;}g->struct_size')
fixture_path = out / 'quick-handoff-fixture.c'
fixture_path.write_text(fixture)
for sanitized in (False, True):
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    for name, feature_flags, source in (
        ('reducer', [], 'frequency_native_input_test.c'),
        ('direct', selection + ['-DPORTABLE_RETURN_APP="default.elf"'], 'frequency_native_adapter_test.c'),
        ('snapshot', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_RASTER_SNAPSHOT'], 'frequency_native_adapter_test.c'),
        ('quick-direct', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_QUICK_NATIVE'], 'frequency_native_adapter_test.c'),
        ('quick-snapshot', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_QUICK_NATIVE', '-DPORTABLE_RASTER_SNAPSHOT'], 'frequency_native_adapter_test.c'),
    ):
        label = name + ('-sanitized' if sanitized else '-normal')
        binary = out / label
        sources = [str(root / 'test/native_apps' / source)]
        if name.startswith('quick-'):
            sources += [str(root / 'lib/PortableApps/src' / name) for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c')]
            feature_flags += ['-DFREQUENCY_HANDOFF_FIXTURE=' + json.dumps(str(fixture_path))]
        subprocess.run([*common, *flags, *feature_flags, '-Wno-unused-function', *sources, '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=30,
            env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (out / (label + '.log')).write_text(result.stdout + result.stderr)
        print(label + ': ' + result.stdout + result.stderr, end='', flush=True)
        result.check_returncode()

# Statically linked app hooks must not expand the public dynamic ABI. Compile
# real adapter objects with and without snapshots and inherited Quick controls.
for snapshot in (False, True):
    for quick in (False, True):
        obj = out / f'adapter-snapshot{int(snapshot)}-quick{int(quick)}.o'
        flags = selection + (['-DPORTABLE_RASTER_SNAPSHOT'] if snapshot else [])
        if quick:
            flags += ['-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT']
        subprocess.run([*common, '-Wno-unused-function', *flags, '-c', str(root / 'lib/PortableApps/src/adapter.c'), '-o', str(obj)], check=True)
        symbols = subprocess.check_output(['readelf', '--wide', '--symbols', str(obj)], text=True)
        for name in ('available', 'begin', 'layout', 'present', 'ready', 'repaint', 'back'):
            rows = [line for line in symbols.splitlines() if line.split()[-1:] == ['portable_frequency_native_' + name]]
            assert len(rows) == 1 and 'GLOBAL HIDDEN' in rows[0], rows
print('PASS: ten normal/sanitized executions and seven private Frequency hooks hidden in four adapter compositions')
