#!/usr/bin/env python3
"""Check the selected Settings API using the target's exact SDK and defines.

Capability providers are simulated; this is not a hardware qualification.
Also exercise the existing scrolling fixture with the selected build policy.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import test_native_time_settings as native

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target-dir',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/home-desk-lock-tests')
    args=parser.parse_args();target=args.target_dir.resolve();out=args.output_dir.resolve()
    out.mkdir(parents=True,exist_ok=True)
    record=json.loads((target/'settings-build-record.json').read_text())
    assert record['version']==('1.3.15' if record.get('ble_broadcast') else '1.3.14') and record['home_desk_lock']['readonly']
    assert record['sleep_modes']==[] and record['mode_capabilities']['sleep_preference_modes']==[]
    include=target/'performance-sdk/include'
    for name,expected in record['performance_trace']['sdk_headers'].items():
        assert hashlib.sha256((include/name).read_bytes()).hexdigest()==expected,name
    for name,expected in record['tagged_alarm_sdk']['sha256'].items():
        if name!='LICENSE':assert hashlib.sha256((include/name).read_bytes()).hexdigest()==expected,name
    sources=[ROOT/'Apps/settings_native_entry.c',*[ROOT/'lib/PortableApps/src'/name for name in
        native.HELPERS+['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]]
    receipt={'hardware':'not run','target_sha256':record['sha256'],
             'runtime_ref':record['native_time_runtime_commit'],'runs':{}}
    for short in (False,True):
        for sanitized in (False,True):
            label=('short' if short else 'paper')+('-san' if sanitized else '')
            # The shared fixture already declares these empty feature macros.
            fixture_defines={'PORTABLE_SETTINGS_APP','PORTABLE_SETTINGS_NATIVE_TIME',
                'PORTABLE_SETTINGS_TIME_ZONE','PORTABLE_SETTINGS_X4_DESK_CLOCK',
                'PORTABLE_SLEEP_SETTINGS','PORTABLE_INPUT_NAVIGATION','PORTABLE_QUICK_ACTIONS',
                'PORTABLE_QUICK_RADIOS','PORTABLE_ALARM_CLIENT','PORTABLE_ALARM_SETTINGS'}
            assert all('-D'+name in record['build_defines'] for name in fixture_defines)
            flags=[flag for flag in record['build_defines'] if flag[2:] not in fixture_defines]+['-DTEST_NATIVE_SETTINGS_QUICK']
            if short:flags+=['-DTEST_NATIVE_SETTINGS_SHORT']
            if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
            common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function',*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include')]
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            log=out/(label+'.log');results=[]
            with log.open('w') as stream:
                fixtures=[('home_desk_lock_settings_test.c',list(map(str,range(6)))),
                          ('touch_scroll_settings_test.c',['drag','select','back','flipped'])]
                if record.get('ble_broadcast'):fixtures.append(('broadcast_storage_guard_test.c',['0','1']))
                for fixture,cases in fixtures:
                    binary=out/(label+'-'+fixture.removesuffix('.c'))
                    subprocess.run([*common,ROOT/'test/native_apps'/fixture,*sources,'-Wl,--wrap=free','-o',binary],check=True)
                    for case in cases:
                        run_env=env.copy()
                        if fixture=='home_desk_lock_settings_test.c' and case=='0' and not sanitized:
                            frames=out/(label+'-frames');frames.mkdir(exist_ok=True)
                            for old in frames.glob('*.pbm'):old.unlink()
                            run_env['NATIVE_SETTINGS_FRAMES']=str(frames)
                        result=subprocess.run([binary,case],timeout=20,env=run_env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
                        stream.write(result.stdout);stream.flush()
                        if result.returncode:print(result.stdout,flush=True)
                        result.check_returncode();results.append({'fixture':fixture,'case':case})
            receipt['runs'][label]=results
            print(label,len(results),'production controller cases passed',flush=True)
    receipt['process_cases']=sum(map(len,receipt['runs'].values()))
    receipt['source_sha256']={str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest()
        for path in [ROOT/'lib/PortableApps/src/settings.inc',ROOT/'scripts/build_portable_settings.py',
                     ROOT/'test/native_apps/home_desk_lock_settings_test.c',Path(__file__).resolve()]}
    (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')

if __name__=='__main__':main()
