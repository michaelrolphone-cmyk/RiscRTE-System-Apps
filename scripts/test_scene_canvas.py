#!/usr/bin/env python3
"""Exercise the single-provider scene/canvas custody boundary with physical doubles."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
MODES=('prefix','scene-busy','scene-fault','switch','lease','receipt','reentrant','canvas-fault','replacement','null','open-native','submit-native','stale')
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);inc=stage_sdk(a.runtime.resolve(),ROOT,out/'sdk')
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if a.sanitize:flags+=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
 rows=[]
 for name,extra,profiles in [('full',[],('watch','large','paper')),('color-dynamic',['-DSCENE_HOST_TELEMETRY=0','-DEXPECT_COMPACT'],('watch','large','paper')),('watch-compact',['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1','-DEXPECT_COMPACT'],('watch',))]:
  exe=out/name;cmd=[os.environ.get('CC','cc'),*flags,*extra,'-I'+str(inc),str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene_canvas/composition_test.c'),'-o',str(exe)]
  with (out/(name+'-build.log')).open('w') as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
  for profile in profiles:
   for mode in MODES:
    log=out/(name+'-'+profile+'-'+mode+'.log')
    with log.open('w') as f:subprocess.run([str(exe),mode,profile],stdout=f,stderr=subprocess.STDOUT,env=env,check=True)
    rows.append({'build':name,'profile':profile,'mode':mode,'passed':True})
 files=[ROOT/'Services/scene_host/host.c',ROOT/'Services/scene_host/scene_canvas.inc',ROOT/'sdk/app/RiscSceneCanvasV1.h',ROOT/'test/scene_canvas/composition_test.c',ROOT/'test/scene/host_test.c',Path(__file__).resolve(),ROOT/'scripts/scene_sdk.py',*sorted((ROOT/'Services/canvas_host').glob('*.h')),*sorted(inc.glob('*.h'))]
 (out/'results.json').write_text(json.dumps({'sanitize':a.sanitize,'cases':rows,'scope':'production scene and canvas composition; physical/Runtime endpoints doubled','sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
 print(str(len(rows))+' scene/canvas composition cases PASS')
if __name__=='__main__':main()
