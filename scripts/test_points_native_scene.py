#!/usr/bin/env python3
"""Points facade and shared real scene adapter: synthetic providers, no hardware."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--alarm-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
 previous=O/'receipt.json'
 if previous.exists():previous.rename(O/('receipt-previous-'+hashlib.sha256(previous.read_bytes()).hexdigest()[:12]+'.json'))
 shutil.copytree(S/'lib/PortableApps/time',O/'time',dirs_exist_ok=True)
 inc=O/'include';shutil.copytree(S/'lib/PortableApps/include',inc,dirs_exist_ok=True)
 for f in (a.runtime/'sdk/app').glob('*.h'):shutil.copyfile(f,inc/f.name)
 for n in ['AlarmServiceV1.h','AlarmServiceV2.h']:shutil.copyfile(a.alarm_sdk/n,inc/n)
 flags=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 flags+=['-I'+str(inc),'-I'+str(S/'lib/NativeApps/include')]
 rows=[]
 quick=['-DTEST_SCENE_QUICK','-DPORTABLE_QUICK_NATIVE_GEOMETRY','-DPORTABLE_QUICK_SAFE_CONTENT','-DPORTABLE_QUICK_CAPABILITY_SAFE','-DPORTABLE_WATCH_NATIVE_HEIGHT']
 for name,extra,cases in [('state',[],[None]),('adapter',[],[0,1,2,8,9,10,14]),('light',['-DTEST_SCENE_LIGHT'],[3,4,5,11,12,13]),('alarm',['-DTEST_SCENE_ALARM'],[6,7]),('quick',quick,[15,16,17]),('quick-round',quick+['-DTEST_SCENE_ROUND','-DPORTABLE_SAFE_VIEWPORT','-DPORTABLE_WATCH_SAFE_INSETS'],[18,19,20])]:
  src=S/'test/native_apps'/('native_scene_state_test.c' if name=='state' else 'points_native_scene_test.c');exe=O/name
  deps=[str(S/'lib/PortableApps/src'/x) for x in ['quick_actions.c','quick_render.c','quick_session.c']] if name.startswith('quick') else []
  cmd=[*flags,*extra,str(src),*deps,'-o',str(exe)];subprocess.run(cmd,check=True)
  for case in cases:
   r=subprocess.run([str(exe),*([str(case)] if case is not None else [])],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'},timeout=30)
   (O/f'{name}-{case}.log').write_text(r.stdout+r.stderr);r.check_returncode();print(r.stdout,end='',flush=True);rows.append({'name':name,'case':case,'passed':True})
 files=[S/'lib/PortableApps/src'/n for n in ['adapter.c','native_scene.inc','foreground_idle.inc','foreground_adapter_open.inc','quick_adapter.inc','alarm.inc','quick_actions.c','quick_render.c','quick_session.c']]+[S/'lib/PortableApps/include'/n for n in ['PortableNativeScene.h','PortableNativeSceneState.h','PortableTimecardNative.h','PortablePointsNative.h']]+[S/'test/native_apps'/n for n in ['native_scene_state_test.c','native_scene_adapter_test.c','points_native_scene_test.c']]+[Path(__file__)]
 receipt={'passed':True,'sanitized':a.sanitize,'runs':rows,'hardware':False,'scope':'Actual shared adapter and foreground/alarm helpers; display/raw input/Runtime/Light and alarm-service media are doubles. Not target or product qualification.','source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
 (O/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()
