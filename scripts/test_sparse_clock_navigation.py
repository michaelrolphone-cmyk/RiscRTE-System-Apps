#!/usr/bin/env python3
"""Run the staged native sparse Clock sources through raw navigation and custody.

The candidate build receipt must match every production input and SDK header.
Host providers model raw touch, navigation, time, display and Runtime. The Clock,
shared adapter, gesture controller and X4 sleep owner are production sources.
This does not execute the Xtensa ELF or qualify hardware.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CUSTODY_CASES = ('terminal cold gpio invalid-record unset absent-native invalid-native refused held cancel '
 'promotion-failed promotion-partial promotion-retained promotion-ready record-context native-context '
 'native-release native-acquire-retained rtc-acquire-retained rtc-read-retained panel-retained panel-refused '
 'retained resume-retained clear-retained release-retained key-retained stage-context stage-refused alarm-due '
 'slow-prepare cross-minute missing-zone missing-basis bad-zone bad-basis manual alarm-output alarm-uncertain '
 'refused-promotion-failed refused-promotion-partial init-nosuffix init-missing-barrier').split()
NAVIGATION_CASES = ('raw-swipe-right raw-swipe-left raw-swipe-up raw-swipe-down raw-top-replay '
 'raw-held-swipe raw-center raw-quick-close raw-quick-center raw-quick-back raw-quick-crown '
 'raw-quick-dismiss-swipe').split()
INSPECTION_CASES = ['raw-quick-inspect-'+dismiss for dismiss in ('close','center','back','crown','dismiss-swipe')]
NAVIGATION_CASES += INSPECTION_CASES + ['raw-quick-brightness']
ASYNC_CASES = [case for case in NAVIGATION_CASES if 'quick' in case]
FLAGS = ('TEST_NATIVE_LANDSCAPE PORTABLE_DISPLAY_ROTATION=90 PORTABLE_APP_OWNS_TOUCH_CHROME '
 'PORTABLE_RTC_WALL_TIME PORTABLE_ALARM_CLIENT PORTABLE_INPUT_NAVIGATION PORTABLE_APP_SLEEP_LOCAL '
 'PORTABLE_CROWN_SLEEP_LOCAL PORTABLE_SLEEP_MANUAL_ONLY PORTABLE_DESK_CLOCK PORTABLE_DESK_CLOCK_SPARSE_START '
 'ALARM_SERVICE_TAGGED_V2 PORTABLE_QUICK_ACTIONS PORTABLE_QUICK_RADIOS').split()
NAMES = ('adapter.c desk_clock_faces.c PortableRealtimeClient.c PortableTimeZone.c PortableTimeZoneCatalog.c '
         'PortableTimeZonePreference.c quick_actions.c quick_render.c quick_session.c quick_radios.c').split()

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-source', type=Path, required=True, help='Exact System production source used for candidate Clock')
    parser.add_argument('--candidate-clock', type=Path, required=True, help='Staged default input directory with build-evidence.json and desk-sdk')
    parser.add_argument('--x4', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/sparse-clock-navigation')
    args = parser.parse_args()
    system=args.system_source.resolve();candidate=args.candidate_clock.resolve();x4=args.x4.resolve()
    runtime=args.runtime.resolve();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    receipt=json.loads((candidate/'build-evidence.json').read_text());include=candidate/'desk-sdk/include'
    assert receipt['sparse_start'] and receipt['quick_actions'] and receipt['quick_radios']
    assert receipt['tagged_alarm_sdk']['api']==2
    for name,expected in receipt['desk_sources'].items():
        assert digest(system/name)==expected, 'Candidate source differs: '+name
    for name,expected in receipt['desk_sdk_headers'].items():
        assert digest(include/name)==expected, 'Candidate SDK differs: '+name
    sleep=x4/'minimal/apps/portable_sleep.c'
    assert digest(sleep)==receipt['local_sleep_source_sha256'], 'Candidate sleep owner differs'
    assert digest(candidate/'default.elf')==receipt['sha256'], 'Candidate ELF differs'
    sources=[system/'Apps/paper_clock.c',*[system/'lib/PortableApps/src'/name for name in NAMES],
             sleep,ROOT/'test/native_apps/sparse_clock_startup_test.c']
    evidence={'purpose':'actual staged native sparse Clock sources with host provider doubles',
              'candidate_elf_sha256':receipt['sha256'],'candidate_system_commit':receipt['repository_commit'],
              'candidate_working_tree_dirty':receipt['working_tree_dirty'],
              'candidate_production_sources':receipt['desk_sources'],
              'candidate_production_inputs_verified':True,'sleep_source_sha256':digest(sleep),
              'test_source_sha256':digest(sources[-1]),'hardware':'not run','runs':[]}
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for sanitized,manual_only in ((False,True),(True,True),(False,False),(True,False)):
        label=('asan-ubsan' if sanitized else 'normal')+('' if manual_only else '-auto-idle');binary=out/label
        flags=[flag for flag in FLAGS if manual_only or flag!='PORTABLE_SLEEP_MANUAL_ONLY']
        normal_cases=CUSTODY_CASES+NAVIGATION_CASES if manual_only else INSPECTION_CASES
        async_cases=ASYNC_CASES if manual_only else INSPECTION_CASES
        extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,
                        *['-D'+flag for flag in flags],'-I'+str(include),'-I'+str(system/'lib/NativeApps/include'),
                        '-I'+str(runtime/'sdk/driver'),'-I'+str(x4/'minimal/drivers/x4pro_power'),
                        *map(str,sources),'-o',str(binary)],check=True)
        with (out/(label+'.log')).open('w') as log:
            for asynchronous,cases in ((False,normal_cases),(True,async_cases)):
                for case in cases:
                    state=out/'state';state.unlink(missing_ok=True)
                    case_env=dict(env)
                    case_env.pop('RAW_ASYNC',None)
                    if asynchronous:case_env['RAW_ASYNC']='1'
                    if case=='raw-quick-brightness':case_env['RAW_QUICK_FRAME']=str(out/(label+('-busy' if asynchronous else '-reference')+'.pixels'))
                    subprocess.run([binary,case,state],env=case_env,stdout=log,stderr=log,check=True,timeout=20)
                    evidence['runs'].append({'profile':label,'case':case,'async_presentation':asynchronous,'manual_only_sleep':manual_only})
        if manual_only:assert (out/(label+'-busy.pixels')).read_bytes()==(out/(label+'-reference.pixels')).read_bytes(), 'Brightness pixels lag the applied final value'
        print(label+': '+str(len(normal_cases)+len(async_cases))+' source-matched navigation/custody cases passed',flush=True)
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(str(len(evidence['runs']))+' exact-source host cases PASS: '+str(out/'evidence.json'))

if __name__=='__main__':
    main()
