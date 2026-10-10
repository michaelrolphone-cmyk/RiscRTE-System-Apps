#!/usr/bin/env python3
"""Selected Settings scrolling through production controller and MONO1 renderer.
Provider timings are simulated, never physical display performance claims.
"""
import argparse, hashlib, json, os, shutil, subprocess, tempfile
from pathlib import Path
import test_native_time_settings as native
import portable_alarm_build
ROOT=Path(__file__).resolve().parents[1]
CASES='drag regions select busy-hit stop-tap reverse bounds back home horizontal replaced cancelled footer-drag modal flipped queued-up reentry'.split()
def run(command, **kwargs):
    return subprocess.run(list(map(str,command)),check=True,**kwargs)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--output-dir',type=Path,default=ROOT/'build/touch-scroll-settings')
    p.add_argument('--case',action='append',choices=CASES)
    p.add_argument('--normal-only',action='store_true')
    portable_alarm_build.options(p)
    a=p.parse_args();a.alarm_client=True;out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    receipt={'hardware':'not run','runtime_ref':native.RUNTIME_REF,'runs':{}}
    unit=out/'controller'
    run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function',
         '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie',
         '-I'+str(ROOT/'lib/PortableApps/include'),ROOT/'test/native_apps/portable_touch_scroll_test.c','-o',unit])
    run([unit],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
    with tempfile.TemporaryDirectory(prefix='touch-scroll-settings-') as tmp:
        stage=Path(tmp);include=stage/'include'
        shutil.copytree(ROOT/'lib/PortableApps/include',include)
        shutil.copytree(ROOT/'lib/PortableApps/time',stage/'time')
        for name in native.HEADERS:
            data=subprocess.check_output(['git','-C',a.runtime,'show',native.RUNTIME_REF+':sdk/app/'+name])
            (include/name).write_bytes(data)
        tagged=portable_alarm_build.stage(a,p,out,include)
        if tagged:receipt["tagged_alarm_sdk"]=tagged
        for short in [False,True]:
            for sanitized in [False] if a.normal_only else [False,True]:
                label=('short' if short else 'paper')+('-san' if sanitized else '')
                binary=out/label
                flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
                if tagged:flags+=['-DALARM_SERVICE_TAGGED_V2']
                if short:flags+=['-DTEST_NATIVE_SETTINGS_SHORT']
                sources=[ROOT/'Apps/settings_native_entry.c',ROOT/'test/native_apps/touch_scroll_settings_test.c',*[ROOT/'lib/PortableApps/src'/name for name in native.HELPERS+['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]]
                run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',
                     '-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_TOUCH_SCROLL','-DTEST_NATIVE_SETTINGS_QUICK',
                     *flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),*sources,'-Wl,--wrap=free','-o',binary])
                results=[]
                for case in a.case or CASES:
                    frames=out/(label+'-frames')/case
                    frames.mkdir(parents=True,exist_ok=True)
                    r=run([binary,case,*([frames] if not sanitized else [])],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),text=True,stdout=subprocess.PIPE,timeout=20)
                    result=json.loads(r.stdout);results.append(result);print(label,case,result,flush=True)
                receipt['runs'][label]=results
    receipt['source_sha256']={str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in [ROOT/'lib/PortableApps/include/PortableTouchScroll.h',ROOT/'lib/PortableApps/include/PortableTouch.h',ROOT/'lib/PortableApps/src/adapter.c',*ROOT.glob('lib/PortableApps/src/settings*.inc'),ROOT/'test/native_apps/touch_scroll_settings_test.c',ROOT/'test/native_apps/portable_touch_scroll_test.c',ROOT/'scripts/build_portable_settings.py',Path(__file__).resolve()]}
    receipt['source_commit']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    receipt['working_tree_dirty']=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())
    receipt['process_cases']=sum(map(len,receipt['runs'].values()))
    (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()
