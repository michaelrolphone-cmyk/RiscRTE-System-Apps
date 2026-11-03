#!/usr/bin/env python3
"""Production pointer surface, gesture and unchanged held-input qualification."""
import argparse,json,os,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
CASES=('invalid','previsible','tap','move','right','scroll','drag','drag_idle','held_mix','route_visible','batch_tap','idle_gap','move_home','gap','resync','update','close','pause','outside','local_fault','native_loss')
def run(runtime,out,sanitize):
 out.mkdir(parents=True,exist_ok=True)
 inc=stage_sdk(runtime,ROOT,out/'sdk')
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if sanitize:flags+=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
 rows=[]
 for profile,pf in [('full',[]),('compact',['-DSCENE_HOST_TELEMETRY=0']),('watch',['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1'])]:
  exe=out/profile
  command=[os.environ.get('CC','cc'),*flags,*pf,'-I'+str(inc),str(ROOT/'test/pointer/host_fixture.c'),str(ROOT/'test/pointer/provider_test.c'),'-o',str(exe)]
  p=subprocess.run(command,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  (out/(profile+'-compile.log')).write_text(p.stdout)
  if p.returncode:print(p.stdout);p.check_returncode()
  for shape in ([1] if profile=='watch' else range(6)):
   for case in CASES:
    p=subprocess.run([str(exe),str(shape),case],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/(profile+'-'+str(shape)+'-'+case+'.log')).write_text(p.stdout)
    if p.returncode:print(p.stdout);p.check_returncode()
    rows.append({'profile':profile,'shape':shape,'case':case})
 (out/'results.json').write_text(json.dumps({'passed':rows,'sanitize':sanitize},indent=2)+'\n')
 print(len(rows),'production pointer cases PASS')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 run(a.runtime.resolve(),a.output.resolve(),a.sanitize)
