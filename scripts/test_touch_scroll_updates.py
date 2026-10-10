#!/usr/bin/env python3
"""Actual paper update/controller touch and release identity qualification."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import portable_native_toolbar_build as native
ROOT=Path(__file__).resolve().parents[1]
CASES='drag fit maximum cleanup-retained select busy-hit stop-tap reverse bounds back home horizontal replaced-contact cancelled touch-retained footer-drag modal queued-up replace version remove confirm-version confirm-remove confirm-hidden back-confirm cancel cancel-progress touch-cancel-progress install connect-version'.split()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--logical-latency',action='store_true');p.add_argument('--flip',action='store_true');p.add_argument('--sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/touch-scroll-updates');p.add_argument('--case',action='append',choices=CASES);p.add_argument('--normal-only',action='store_true');a=p.parse_args()
 out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True);include=out/'sdk/include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',include.parent/'time',dirs_exist_ok=True)
 for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h']:(include/name).write_bytes((a.sdk/name).read_bytes())
 receipt={'hardware':'not run','reader_flip_ui':a.flip,'runs':{}}
 for firmware in [0,1]:
  for san in [False] if a.normal_only else [False,True]:
   label=('firmware' if firmware else 'apps')+('-san' if san else '');binary=out/label
   flags=['-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_APP_TOUCH_SCROLL','-DPORTABLE_UPDATE_TOUCH_SCROLL','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DALARM_SERVICE_TAGGED_V2','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_UPDATE_APP','-DPORTABLE_UPDATE_FIRMWARE='+str(firmware),'-DPORTABLE_WIFI_INSTANCE=15u','-DUPDATE_RETURN_APP="springboard.elf"','-DTEST_IDLE_ELIGIBILITY']
   if a.flip:flags+=['-DTEST_READER_FLIP','-DPORTABLE_PAPER_PREFERENCES']
   if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   sources=[ROOT/('test/native_apps/logical_scroll_updates_latency_test.c' if a.logical_latency else 'test/native_apps/touch_scroll_updates_test.c'),*[ROOT/s for s in native.SOURCES],*[ROOT/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c']]]
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-Wl,--wrap=free','-o',str(binary)],check=True)
   results=[]
   cases=(a.case or ['touch','confirm','install','drag','changed','version','remove','confirm-version','cancel','rapid-taps']) if a.logical_latency else (a.case or CASES)
   for case in cases:
    for latency in ['0','17','2300','never'] if a.logical_latency else [None]:
     frames=out/(label+'-frames')/case;frames.mkdir(parents=True,exist_ok=True)
     arguments=[str(binary),case,latency] if a.logical_latency else [str(binary),case,*([str(frames)] if not san else [])]
     r=subprocess.run(arguments,check=True,text=True,stdout=subprocess.PIPE,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
     result=json.loads(r.stdout);results.append(result);print(label,case,result,flush=True)
   receipt['runs'][label]=results
 receipt['process_cases']=sum(map(len,receipt['runs'].values()));receipt['source_commit']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 receipt['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Apps/update_scroll.inc',ROOT/'Apps/update_portable.inc',ROOT/'test/native_apps/touch_scroll_updates_test.c',ROOT/'test/native_apps/logical_scroll_updates_latency_test.c',ROOT/'test/native_apps/logical_touch_queue_fixture.h',Path(__file__).resolve()]}
 (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()
