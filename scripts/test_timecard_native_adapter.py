#!/usr/bin/env python3
"""Qualify private Timecard owner14 actual direct/snapshot/Quick adapter wiring."""
import os
import argparse
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--alarm-api1', type=Path, help='Canonical Utilities AlarmServiceV1.h with the sleep-resume suffix')
a = parser.parse_args()
out = root / ('build/timecard-native-adapter-canonical-api1' if a.alarm_api1 else 'build/timecard-native-adapter')
out.mkdir(parents=True, exist_ok=True)
common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
    *('-I' + str(root / p) for p in ('lib/PortableApps/include', 'lib/PortableApps/src', 'lib/NativeApps/include'))]
if a.alarm_api1:
    assert a.alarm_api1.is_file()
    assert 'ALARM_SERVICE_SLEEP_RESUME_SUPPORTED' in a.alarm_api1.read_text()
    common += ['-include', str(a.alarm_api1.resolve())]
selection = ['-D' + flag for flag in ('PORTABLE_TIMECARD_NATIVE_UI', 'PORTABLE_NOVA_UI',
    'PORTABLE_NATIVE_CANVAS', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_RETAINED_RGB565_HANDOFF')]
# Extend the existing provider fixture only in the generated Quick test copy.
fixture = (root / 'test/native_apps/portable_handoff_test.c').read_text()
fixture = fixture.replace('#include "../../lib/PortableApps/src/adapter.c"',
    '#include ' + json.dumps(str(root / 'lib/PortableApps/src/adapter.c')))
alarm = '''static int32_t timecard_alarm_status(void*c,alarm_status_v1*s){(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t timecard_alarm_step(void*c){(void)c;return ALARM_OK;}
static int32_t timecard_alarm_ack(void*c,const alarm_token_v1*t){(void)t;return timecard_alarm_step(c);}
static int32_t timecard_alarm_sleep(void*c,alarm_sleep_v1*s){(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 timecard_alarm={1,sizeof(timecard_alarm),NULL,timecard_alarm_status,timecard_alarm_step,timecard_alarm_step,timecard_alarm_ack,timecard_alarm_sleep,timecard_alarm_step};
'''
if a.alarm_api1:
    alarm = alarm.replace('static const alarm_service_v1 timecard_alarm={1,sizeof(timecard_alarm),NULL,timecard_alarm_status,timecard_alarm_step,timecard_alarm_step,timecard_alarm_ack,timecard_alarm_sleep,timecard_alarm_step};',
        'static unsigned timecard_resume_calls;\nstatic int32_t timecard_alarm_resume(void*c,const alarm_sleep_v1*s){(void)c;(void)s;timecard_resume_calls++;return ALARM_OK;}\nstatic const alarm_service_sleep_v1 timecard_alarm={{1,sizeof(timecard_alarm),NULL,timecard_alarm_status,timecard_alarm_step,timecard_alarm_step,timecard_alarm_ack,timecard_alarm_sleep,timecard_alarm_step},timecard_alarm_resume};')
needle = 'static bool mock_acquire(const char *name'
assert needle in fixture
fixture = fixture.replace(needle, alarm + needle)
needle = 'else if(!strcmp(name,"input.touch.raw"))g->api=&mock_touch;'
assert needle in fixture
fixture = fixture.replace(needle, needle + '\n    else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))g->api=&timecard_alarm;')
fixture = fixture.replace('assert(version==1 && !instance);g->struct_size',
    'if(version!=1||instance){return false;}g->struct_size')
fixture_path = out / 'quick-handoff-fixture.c'
fixture_path.write_text(fixture)
for sanitized in (False, True):
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    for name, feature_flags, source in (
        ('direct', selection + ['-DPORTABLE_RETURN_APP="default.elf"'], 'timecard_native_adapter_test.c'),
        ('snapshot', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_RASTER_SNAPSHOT'], 'timecard_native_adapter_test.c'),
        ('quick-direct', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_QUICK_NATIVE'], 'timecard_native_adapter_test.c'),
        ('quick-snapshot', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_QUICK_NATIVE', '-DPORTABLE_RASTER_SNAPSHOT'], 'timecard_native_adapter_test.c'),
        *[(name + '-owners9-14', flags + ['-DPORTABLE_' + owner + '_NATIVE_UI' for owner in ('FILES', 'FREQUENCY', 'LORA', 'POINTS', 'STOPWATCH')], 'timecard_native_adapter_test.c')
          for name, flags in (
            ('direct', selection + ['-DPORTABLE_RETURN_APP="default.elf"']),
            ('snapshot', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_RASTER_SNAPSHOT']),
            ('quick-direct', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_QUICK_NATIVE']),
            ('quick-snapshot', selection + ['-DPORTABLE_RETURN_APP="default.elf"', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_QUICK_NATIVE', '-DPORTABLE_RASTER_SNAPSHOT']))],
    ):
        label = name + ('-sanitized' if sanitized else '-normal')
        binary = out / label
        sources = [str(root / 'test/native_apps' / source)]
        if name.startswith('quick-'):
            sources += [str(root / 'lib/PortableApps/src' / name) for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c')]
            feature_flags += ['-DTIMECARD_HANDOFF_FIXTURE=' + json.dumps(str(fixture_path))]
        subprocess.run([*common, *flags, *feature_flags, '-Wno-unused-function', *sources, '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=30,
            env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (out / (label + '.log')).write_text(result.stdout + result.stderr)
        print(label + ': ' + result.stdout + result.stderr, end='', flush=True)
        result.check_returncode()
        binary.unlink()

# Statically linked app hooks must not expand the public dynamic ABI. Compile
# real adapter objects with idle activity fences and with/without snapshots and Quick.
for snapshot in (False, True):
    for quick in (False, True):
        obj = out / f'adapter-snapshot{int(snapshot)}-quick{int(quick)}.o'
        flags = selection + ['-DPORTABLE_APP_SLEEP_LOCAL', '-DPORTABLE_ALARM_CLIENT'] + (['-DPORTABLE_RASTER_SNAPSHOT'] if snapshot else [])
        if quick:
            flags += ['-DPORTABLE_QUICK_ACTIONS']
            if a.alarm_api1 and snapshot:
                flags += ['-DPORTABLE_WATCH_COLOR_ONLY']
        subprocess.run([*common, '-Wno-unused-function', *flags, '-c', str(root / 'lib/PortableApps/src/adapter.c'), '-o', str(obj)], check=True)
        symbols = subprocess.check_output(['readelf', '--wide', '--symbols', str(obj)], text=True)
        for name in ('available', 'begin', 'layout', 'present', 'ready', 'repaint', 'back', 'contact'):
            rows = [line for line in symbols.splitlines() if line.split()[-1:] == ['portable_timecard_native_' + name]]
            assert len(rows) == 1 and 'GLOBAL HIDDEN' in rows[0], rows
        obj.unlink()
print('PASS: sixteen normal/sanitized executions including combined owners9-14 and eight private Timecard hooks hidden in four adapter compositions')
