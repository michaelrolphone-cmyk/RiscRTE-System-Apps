#!/usr/bin/env python3
"""Opt-in Settings availability and draft boundaries, actual portable controller."""
import argparse,json,os,subprocess,shutil
from pathlib import Path
import build_portable_settings as builder
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=json.loads((a.target/'settings-build-record.json').read_text())
includes=a.output.resolve()/'sdk/include';shutil.copytree(a.target/'native-time-sdk/include',includes,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',includes.parent/'time',dirs_exist_ok=True)
for name in ('PortableSettingsAvailability.h','RiscKeyValueAvailabilityV1.h'):shutil.copyfile(ROOT/'lib/PortableApps/include'/name,includes/name)
fixture=set('PORTABLE_SETTINGS_APP PORTABLE_SETTINGS_NATIVE_TIME PORTABLE_SETTINGS_TIME_ZONE PORTABLE_SETTINGS_X4_DESK_CLOCK PORTABLE_SLEEP_SETTINGS PORTABLE_INPUT_NAVIGATION PORTABLE_ALARM_CLIENT'.split())
flags=[f for f in r['build_defines'] if f[2:] not in fixture]+['-DTEST_NATIVE_SETTINGS_ALARMS','-DPORTABLE_SETTINGS_AVAILABILITY']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
sources=[ROOT/'test/native_apps/settings_availability_workflow_test.c',ROOT/'Apps/settings_native_entry.c',*[ROOT/s for s in builder.NATIVE_TIME_SOURCES]]
exe=a.output/'settings-availability';cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-Wl,--wrap=free','-o',str(exe)]
subprocess.run(cmd,check=True)
for case in 'zone-touch-cancel date-discard storage-unavailable storage-pending storage-conflict reload-modal unavailable unavailable-modal failed-save-draft conflict draft-pending reload provider-pending available-save malformed status-release-fails invalid-publication'.split():subprocess.run([str(exe),case],check=True,timeout=20,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
