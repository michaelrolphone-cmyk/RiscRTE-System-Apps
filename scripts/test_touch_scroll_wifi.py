#!/usr/bin/env python3
"""Selected Wi-Fi lists through the actual controller, adapter and MONO1 renderer."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
import portable_native_toolbar_build as native
ROOT=Path(__file__).resolve().parents[1]
CASES='root drag select busy-hit stop-tap reverse bounds back home horizontal replaced cancelled touch-retained footer-drag modal queued-up cleanup refresh keyboard confirm-hidden'.split()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--flip',action='store_true');p.add_argument('--ble-broadcast',action='store_true');p.add_argument('--sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/touch-scroll-wifi');p.add_argument('--case',action='append',choices=CASES);p.add_argument('--normal-only',action='store_true');a=p.parse_args()
 out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True);include=out/'sdk/include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',include.parent/'time',dirs_exist_ok=True)
 for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h']:(include/name).write_bytes((a.sdk/name).read_bytes())
 receipt={'hardware':'not run','reader_flip_ui':a.flip,'runs':{}}
 for short in [False,True]:
  for san in [False] if a.normal_only else [False,True]:
   label=('short' if short else 'paper')+('-san' if san else '');binary=out/label
   flags=['-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DALARM_SERVICE_TAGGED_V2','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_WIFI_SETTINGS_APP','-DPORTABLE_WIFI_INSTANCE=15u','-DPORTABLE_WIFI_STORAGE_INSTANCE=6','-DWIFI_RETURN_APP="springboard.elf"']
   if a.ble_broadcast:flags+=['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES']
   if a.flip:flags+=['-DTEST_READER_FLIP','-DPORTABLE_PAPER_PREFERENCES']
   if short:flags+=['-DTEST_TOOLBAR_NATIVE_WIDTH=600','-DTEST_TOOLBAR_NATIVE_HEIGHT=400']
   if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   sources=[ROOT/'test/native_apps/touch_scroll_wifi_entry.c',ROOT/'test/native_apps/touch_scroll_wifi_test.c',*[ROOT/s for s in native.SOURCES],*[ROOT/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c']]]
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-Wl,--wrap=free','-o',str(binary)],check=True)
   results=[]
   for case in a.case or CASES:
    frames=out/(label+'-frames')/case;frames.mkdir(parents=True,exist_ok=True)
    r=subprocess.run([str(binary),case,*([str(frames)] if not san else [])],check=True,text=True,stdout=subprocess.PIPE,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    result=json.loads(r.stdout);results.append(result);print(label,case,result,flush=True)
   receipt['runs'][label]=results
 receipt['process_cases']=sum(map(len,receipt['runs'].values()));receipt['source_commit']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()
