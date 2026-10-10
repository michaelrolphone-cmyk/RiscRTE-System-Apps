#!/usr/bin/env python3
"""Exercise native RF/audio app-hook finalization and stop-on-retention."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
RUNTIME='30dcec5ce6ce33223f2b203a2399283e1f758567'
CASES=['closed','radio-refused','radio-retained','audio-refused','audio-retained']

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/native-owned-cleanup')
    args=parser.parse_args();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    receipt={'runtime_ref':RUNTIME,'host_cases':{},'hardware':'not run'}
    with tempfile.TemporaryDirectory(prefix='native-owned-cleanup-') as temporary:
        stage=Path(temporary);include=stage/'include'
        shutil.copytree(ROOT/'lib/PortableApps/include',include)
        shutil.copytree(ROOT/'lib/PortableApps/time',stage/'time')
        for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:
            (include/name).write_bytes(subprocess.check_output(['git','-C',args.runtime,'show',RUNTIME+':sdk/app/'+name]))
        for sanitized in (False,True):
            label='asan-ubsan' if sanitized else 'normal';binary=out/label
            san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
            subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,
                '-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),
                str(ROOT/'test/native_apps/native_owned_cleanup_test.c'),*[str(ROOT/'lib/PortableApps/src'/name) for name in
                ['quick_actions.c','quick_render.c','quick_session.c']],'-Wl,--wrap=free','-o',str(binary)],check=True)
            with (out/(label+'.log')).open('w') as log:
                for case in CASES:
                    result=subprocess.run([str(binary),case],check=True,capture_output=True,text=True,
                         env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
                    log.write(result.stdout)
            receipt['host_cases'][label]=CASES
            print(label+': all native app-owned cleanup cases passed')
    (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')

if __name__=='__main__':main()
