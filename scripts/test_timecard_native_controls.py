#!/usr/bin/env python3
"""Existing owners remain unchanged and execute with Timecard selected but idle."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/timecard-native-controls'
out.mkdir(parents=True, exist_ok=True)
baseline = 'ff6151285ee41bdbeff75ebd31ee2621d524e899'
changed = {'lib/PortableApps/src/adapter.c', 'lib/PortableApps/src/raster_snapshot_state.inc',
    'lib/PortableApps/src/quick_adapter.inc', 'lib/PortableApps/src/quick_paper.inc'}
# This prevents a feature-off comparison from accidentally hiding edits in an
# include shared by both preprocessing inputs. Existing Alarm, raw-touch,
# retained custody, ABI headers, renderers and every owner are byte-for-byte.
# The independently reviewed USB native successor changes paper.inc only behind
# the combined USB app/native selectors. Admit that exact blob, never an arbitrary
# edit, and prove every pre-existing branch still emits identical tokens.
usb_paper_blob = '9df172e53dabba2495238a8b9515ee41a10dddbd'
reviewed_successors = []

def verify_usb_paper_successor(name, data, actual_blob):
    assert name == 'lib/PortableApps/src/paper.inc' and actual_blob == usb_paper_blob, name
    old = subprocess.check_output(['git', 'show', baseline + ':' + name], cwd=root, text=True)
    rows = []
    for selectors in ((), ('PORTABLE_USB_TRANSFER_APP',), ('PORTABLE_USB_TRANSFER_NATIVE_UI',)):
        for snapshot in (False, True):
            for toolbar in (False, True):
                defines = ['PORTABLE_TIMECARD_NATIVE_UI', 'PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', *selectors]
                if snapshot:
                    defines += ['PORTABLE_RASTER_SNAPSHOT']
                if toolbar:
                    defines += ['PORTABLE_NATIVE_TIME_TOOLBAR']
                command = [os.environ.get('CC', 'cc'), '-E', '-P', '-x', 'c',
                    *('-D' + flag for flag in defines), '-I' + str(root / 'lib/PortableApps/src'), '-']
                tokens = []
                for source in (old, data.decode()):
                    expanded = subprocess.check_output(command, input=source, text=True)
                    tokens.append('\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''', expanded)))
                assert tokens[0] == tokens[1], ('USB paper successor changed an existing branch', defines)
                assert 'portable_usb_transfer_native_available' not in tokens[1]
                rows.append({'defines': defines, 'identical': True,
                    'token_sha256': hashlib.sha256(tokens[0].encode()).hexdigest()})
    return {'path': name, 'git_blob': actual_blob, 'selector_off_parity': rows}

entries = subprocess.check_output(['git', 'ls-tree', '-r', baseline, 'lib/PortableApps'], cwd=root, text=True)
unchanged = []
for entry in entries.splitlines():
    meta, name = entry.split('\t')
    if name in changed:
        continue
    data = (root / name).read_bytes()
    actual_blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    if actual_blob != meta.split()[2]:
        reviewed_successors.append(verify_usb_paper_successor(name, data, actual_blob))
    else:
        unchanged.append(name)

common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
    *('-I' + str(root / p) for p in ('lib/PortableApps/include', 'lib/PortableApps/src', 'lib/NativeApps/include'))]
fixture = (root / 'test/native_apps/portable_handoff_test.c').read_text().replace(
    '#include "../../lib/PortableApps/src/adapter.c"', '#include ' + json.dumps(str(root / 'lib/PortableApps/src/adapter.c')))
alarm = '''static int32_t control_alarm_status(void*c,alarm_status_v1*s){(void)c;*s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t control_alarm_step(void*c){(void)c;return ALARM_OK;}
static int32_t control_alarm_ack(void*c,const alarm_token_v1*t){(void)t;return control_alarm_step(c);}
static int32_t control_alarm_sleep(void*c,alarm_sleep_v1*s){(void)c;*s=(alarm_sleep_v1){.struct_size=sizeof(*s)};return ALARM_OK;}
static const alarm_service_v1 control_alarm={1,sizeof(control_alarm),NULL,control_alarm_status,control_alarm_step,control_alarm_step,control_alarm_ack,control_alarm_sleep,control_alarm_step};
'''
fixture = fixture.replace('static bool mock_acquire(const char *name', alarm + 'static bool mock_acquire(const char *name')
fixture = fixture.replace('else if(!strcmp(name,"input.touch.raw"))g->api=&mock_touch;',
    'else if(!strcmp(name,"input.touch.raw"))g->api=&mock_touch;\n    else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))g->api=&control_alarm;')
fixture = fixture.replace('assert(version==1 && !instance);g->struct_size', 'if(version!=1||instance){return false;}g->struct_size')
fixture_path = out / 'quick-handoff-fixture.c'
fixture_path.write_text(fixture)
rows = []

def run(label, source, flags, sanitize, extra=()):
    binary = out / label
    sanitized = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitize else []
    subprocess.run([*common, *sanitized, *flags, '-Wno-unused-function', str(root / 'test/native_apps' / source),
        *map(str, extra), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=30,
        env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (out / (label + '.log')).write_text(result.stdout + result.stderr)
    result.check_returncode()
    binary.unlink()
    rows.append(label)
    print(label + ': PASS', flush=True)

for sanitize in (False, True):
    suffix = '-sanitized' if sanitize else '-normal'
    for owner in ('calculator', 'files', 'frequency', 'lora', 'points', 'stopwatch'):
        run(owner + '-reducer' + suffix, owner + '_native_input_test.c', [], sanitize)
    for owner in ('frequency', 'points', 'stopwatch'):
        for snapshot in (False, True):
            for quick in (False, True):
                label = owner + ('-snapshot' if snapshot else '-direct') + ('-quick' if quick else '') + suffix
                flags = ['-D' + f for f in ('PORTABLE_' + owner.upper() + '_NATIVE_UI', 'PORTABLE_TIMECARD_NATIVE_UI',
                    'PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_RETAINED_RGB565_HANDOFF')]
                flags += ['-DPORTABLE_RETURN_APP="default.elf"']
                if snapshot:
                    flags += ['-DPORTABLE_RASTER_SNAPSHOT']
                extra = []
                if quick:
                    flags += ['-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_QUICK_NATIVE',
                        '-D' + owner.upper() + '_HANDOFF_FIXTURE=' + json.dumps(str(fixture_path))]
                    extra = [root / 'lib/PortableApps/src' / name for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c')]
                run(label, owner + '_native_adapter_test.c', flags, sanitize, extra)
(out / 'results.json').write_text(json.dumps({'baseline': baseline, 'unchanged_portable_files': unchanged,
    'reviewed_successors': reviewed_successors, 'passed_executions': rows}, indent=2) + '\n')
print(f'PASS: {len(unchanged)} unchanged PortableApps files and {len(rows)} existing-owner normal/sanitized executions; {len(reviewed_successors)} exact reviewed USB paper successor')
