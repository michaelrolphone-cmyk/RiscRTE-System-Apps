#!/usr/bin/env python3
"""Selected Settings live-controller atomic persistence tests; no device access."""
import argparse,json,os,subprocess
from pathlib import Path
import build_portable_settings as builder
ROOT=Path(__file__).resolve().parents[1]
CASES='''format-save format-touch-save held-touch-save time-touch-save time-cancel-pending time-uncertain cancel-touch-before cancel-touch-pending zone-save face-save direction-save flip-save language-save timer-save time-save
same-value cancel-before cancel-pending cancel-race home-pending submit-busy repeated-confirm
stale-draft stale-unrelated stale-time-before stale-time-during uncertain io capacity
collect-error collect-retained bad-receipt bad-revision submit-dirty-refusal cancel-error
release-error missing-capability bad-api wrong-namespace small-intent read-io read-retained
origin-fence put-fence close-pending rtc-write-false native-seed-io'''.split()
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--short',action='store_true',help='Use the existing 400x600 logical paper fixture')
p.add_argument('--sanitize',action='store_true');p.add_argument('--case',action='append',choices=CASES)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=json.loads((a.target/'settings-build-record.json').read_text());assert r['atomic_settings']['namespace']==1
includes=a.target.resolve()/'native-time-sdk/include'
fixture=set('PORTABLE_SETTINGS_APP PORTABLE_SETTINGS_NATIVE_TIME PORTABLE_SETTINGS_TIME_ZONE PORTABLE_SETTINGS_X4_DESK_CLOCK PORTABLE_SLEEP_SETTINGS PORTABLE_INPUT_NAVIGATION PORTABLE_ALARM_CLIENT'.split())
flags=[f for f in r['build_defines'] if f[2:] not in fixture]+['-DTEST_NATIVE_SETTINGS_ALARMS']
if a.short:flags.append('-DTEST_NATIVE_SETTINGS_SHORT')
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
sources=[ROOT/'test/native_apps/settings_atomic_workflow_test.c',ROOT/'Apps/settings_native_entry.c',*[ROOT/s for s in builder.NATIVE_TIME_SOURCES]]
binary=a.output/'settings_atomic_workflow'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-Wl,--wrap=free','-o',str(binary)],check=True)
for case in a.case or CASES:
 subprocess.run([str(binary),case],check=True,timeout=20,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
print(f'{len(a.case or CASES)} actual Settings controller cases passed; native endpoint scheduled double, hardware not run.')
