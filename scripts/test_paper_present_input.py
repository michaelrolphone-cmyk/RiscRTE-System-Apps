#!/usr/bin/env python3
"""Deterministic ordinary paper submission/controller latency and custody.

Links the real launcher and native adapter, models queued/transfer/BUSY phases,
compares immutable production baseline timing, and runs ASan/UBSan. No hardware,
product artifact edits, installation or publication.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASE='c1ff014792c9ecbc195c402e9b4c3e2c3c9d34d3'
CASES=['latency','repeat','sync','quick','alarm','fini','fallback','status-false',
       'failed','superseded','timeout','fini-timeout','exit-busy','pump-count','stalled-clock']

def run(command,**kwargs):
    return subprocess.run(list(map(str,command)),check=True,**kwargs)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-sdk',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/paper-present')
    args=parser.parse_args();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    receipt={'base':BASE,'fixture_phases_ms':{'queued':[0,40],'transfer':[40,100],'busy':[100,240]},
             'input_poll_ms':20,'runs':{},'hardware':'not run','publication':'none'}
    with tempfile.TemporaryDirectory(prefix='paper-present-') as directory:
        stage=Path(directory);include=stage/'include'
        shutil.copytree(ROOT/'lib/PortableApps/include',include)
        shutil.copytree(ROOT/'lib/PortableApps/time',stage/'time')
        receipt['runtime_sdk_sha256']={}
        for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:
            source=args.runtime_sdk/name;shutil.copyfile(source,include/name)
            receipt['runtime_sdk_sha256'][name]=hashlib.sha256(source.read_bytes()).hexdigest()
        baseline=stage/'baseline';baseline.mkdir()
        data=subprocess.check_output(['git','-C',ROOT,'archive',BASE])
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:archive.extractall(baseline,filter='data')
        # Same test providers/observations; no baseline production source edits.
        for name in ['paper_present_input_test.c','paper_present_controller.c','portable_native_toolbar_test.c']:
            shutil.copyfile(ROOT/'test/native_apps'/name,baseline/'test/native_apps'/name)
        for sanitized in [False,True]:
            san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
            for label,repo,flags,cases in [
                ('baseline',baseline,['-DTEST_BASELINE'],['latency','sync']),
                ('current',ROOT,[],CASES),
                ('launch',ROOT,['-DTEST_APP_COMPATIBLE=true'],['launch-feedback','launch-same','launch-held','launch-retry','launch-replace','launch-cancel','launch-drag','launch-multi','launch-held-entry','launch-failed','launch-status-false','launch-submit-false','launch-timeout','launch-quick','launch-alarm'])]:
                name=label+('-asan-ubsan' if sanitized else '-normal');binary=out/name
                run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*flags,
                     '-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_TOOLBAR_QUICK',
                     '-DPORTABLE_INPUT_NAVIGATION','-I'+str(include),'-I'+str(repo/'lib/NativeApps/include'),
                     repo/'test/native_apps/paper_present_input_test.c',repo/'test/native_apps/paper_present_controller.c',
                     *[repo/'lib/PortableApps/src'/file for file in ['quick_actions.c','quick_render.c','quick_session.c']],
                     '-Wl,--wrap=free','-o',binary])
                results=[]
                for case in cases:
                    case_env=env
                    if label=='launch' and not sanitized and case in ['launch-feedback','launch-same']:
                        frames=out/(name+'-'+case+'-frames');frames.mkdir(exist_ok=True)
                        case_env=dict(env,PAPER_PRESENT_CAPTURE_DIR=str(frames))
                    result=run([binary,case],env=case_env,capture_output=True,text=True,timeout=20)
                    results.append(json.loads(result.stdout))
                receipt['runs'][name]=results
                (out/(name+'.log')).write_text(''.join(json.dumps(r)+'\n' for r in results))
                print(name+': '+str(len(results))+' actual adapter/controller cases passed',flush=True)
    receipt['source_commit']=subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip()
    receipt['source_sha256']={str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in [
        Path(__file__).resolve(),ROOT/'test/native_apps/paper_present_input_test.c',ROOT/'test/native_apps/paper_present_controller.c',
        ROOT/'lib/PortableApps/src/adapter.c',ROOT/'Apps/springboard_paper.inc',ROOT/'Apps/PaperFrame.h']}
    (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Evidence: '+str(out/'evidence.json'))

if __name__=='__main__':main()
