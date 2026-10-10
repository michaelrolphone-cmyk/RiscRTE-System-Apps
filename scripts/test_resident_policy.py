#!/usr/bin/env python3
"""Actual System adapter ELFs through production Runtime, Graph and dlopen."""
import argparse
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--display-sdk', type=Path, required=True)
p.add_argument('--alarm-sdk', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--async-display', action='store_true', help='Exercise the deployed sliced renderer and delayed display completion')
p.add_argument('--modes', nargs='*')
a = p.parse_args()
out = ROOT / 'build/resident-policy'
inc = out / 'include'
shutil.copytree(ROOT / 'lib/PortableApps/include', inc, dirs_exist_ok=True)
shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h'):
    shutil.copyfile(a.runtime / 'sdk/app' / name, inc / name)
if (a.runtime/'sdk/app/RiscFailureEvidenceV1.h').is_file():
    shutil.copyfile(a.runtime/'sdk/app/RiscFailureEvidenceV1.h',inc/'RiscFailureEvidenceV1.h')
for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h', 'RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h'):
    shutil.copyfile(a.display_sdk / name, inc / name)
for name in ('AlarmServiceV1.h', 'AlarmServiceV2.h'):
    shutil.copyfile(a.alarm_sdk / name, inc / name)
san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if a.sanitize else []
base = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, '-I'+str(inc), '-I'+str(ROOT/'lib/NativeApps/include')]
common = ['-DPORTABLE_PAPER_PREFERENCES', '-DPORTABLE_ALARM_TERMINAL_RETENTION', '-DPORTABLE_NATIVE_CUSTODY_FENCE',
          '-DPORTABLE_ALARM_CLIENT', '-DALARM_SERVICE_TAGGED_V2', '-DPORTABLE_INPUT_NAVIGATION',
          '-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_HOME_APP="default.elf"', '-DPORTABLE_RESIDENT_POLICY']
if a.async_display:
    common += ['-DPORTABLE_RASTER_SNAPSHOT']
    base += ['-DPOLICY_ASYNC_DISPLAY']
for role in ('host', 'client'):
    flags = [*common, '-DPORTABLE_RESIDENT_SHELL_'+role.upper(), '-DPORTABLE_APP_SLEEP_LOCAL', '-DPORTABLE_CROWN_SLEEP_LOCAL']
    sources = [ROOT/'test/native_apps/resident_policy_app.c', ROOT/'test/native_apps/resident_policy_adapter.c']
    if role == 'host':
        flags += ['-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_QUICK_RADIOS', '-DPORTABLE_PAPER_TRANSITIONS', '-DPORTABLE_X4_IDLE_POLICY', '-DPORTABLE_LOW_BATTERY']
        sources += [ROOT/'lib/PortableApps/src'/name for name in ('quick_actions.c', 'quick_session.c', 'quick_render.c', 'quick_radios.c')]
    else:
        flags += ['-DPORTABLE_APP_LAUNCH_GUARD', '-DPORTABLE_AUDIO_SESSION', '-DPORTABLE_AUDIO_CONTINUOUS_CAPTURE']
    subprocess.run(['cc', '-std=c11', *base, '-fPIC', '-shared', '-Wl,-Bsymbolic', *flags, *map(str, sources), '-o', str(out/(role+'.elf'))], check=True)
    symbols = subprocess.check_output(['nm', str(out/(role+'.elf'))], text=True)
    assert (' pqa_render' in symbols) == (role == 'host')
    assert ' risc_resident_app_descriptor_v1' in symbols
fixture = ROOT/'test/native_apps/resident_policy_test.c'
caps = [('display.output',1), ('input.touch.raw',1), ('input.navigation',1), ('board.battery',1), ('rtc.clock',2), ('alarm.service',2), ('net.wifi',1), ('bluetooth.hci',1)]
for index, (cap, version) in enumerate(caps, 1):
    subprocess.run(['cc', '-std=c11', *base, '-I'+str(a.runtime/'sdk/driver'), '-fPIC', '-shared',
                    '-DPOLICY_PROVIDER='+str(index), '-DPOLICY_CAPABILITY="'+cap+'"', '-DPOLICY_API='+str(version),
                    str(fixture), '-o', str(out/('provider-'+str(index)+'.elf'))], check=True)
subprocess.run(['cc', '-std=c11', *base, '-DPORTABLE_ALARM_CLIENT', '-DALARM_SERVICE_TAGGED_V2', '-DPORTABLE_X4_IDLE_POLICY',
                '-c', str(fixture), '-o', str(out/'fixture.o')], check=True)
rincs = ['-I'+str(a.runtime/path) for path in ('src', 'sdk/app', 'sdk/driver', 'sdk/hardware', 'lib/ArduinoJson/src', 'test/drivers/stubs')]
rsources = [a.runtime/path for path in ('src/bootstrap/Json.cpp', 'src/bootstrap/Board.cpp', 'src/bootstrap/Runtime.cpp',
            'src/runtime/streams/AppStreamSessions.cpp', 'src/runtime/streams/ProviderQueueHost.cpp',
            'src/runtime/drivers/ProviderGraphV2.cpp', 'src/runtime/drivers/ProviderModuleV2.cpp')]
binary = out/'resident-policy-test'
subprocess.run(['c++', '-std=c++17', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers', *san,
                *rincs, '-rdynamic', *(['-no-pie'] if a.sanitize else []), *map(str, rsources),
                str(ROOT/'test/native_apps/resident_policy_runtime.cpp'), str(out/'fixture.o'), '-ldl', '-o', str(binary)], check=True)
modes = a.modes or ['poll','busy','policy-busy','capture-pending','animation','idle','activity','capture','low-battery','dirty-edit','refused','retained','explicit-sleep']
for mode in modes:
    subprocess.run([str(binary), str(out), mode], check=True, timeout=30,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
