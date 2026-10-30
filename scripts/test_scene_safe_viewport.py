#!/usr/bin/env python3
"""Actual scene provider safe-inset/rotation/touch admission; no hardware claim."""
from pathlib import Path
import argparse,subprocess,json,os,sys,hashlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from scene_sdk import stage_sdk
from build_scene_services import HOST_PROFILE_FLAGS
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);inc=stage_sdk(a.runtime.resolve(),ROOT,out/'sdk');rows=[]
for profile in ('full','color-dynamic'):
 for san in (False,True):
  exe=out/(profile+('-sanitized' if san else '-normal'));cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(inc),'-DHOST_FIXTURE="'+str(ROOT/'test/scene/host_test.c')+'"',*HOST_PROFILE_FLAGS[profile]]
  if san:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  subprocess.run(cmd+[str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/safe_viewport_test.c'),'-o',str(exe)],check=True)
  for rot in (0,90,180,270):
   for w,h,mode in [(368,448,'asymmetric'),(466,466,'round'),(368,448,'invalid'),(368,448,'narrow')]:
    run=subprocess.run([str(exe),str(rot),str(w),str(h),mode],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});rows.append({'profile':profile,'sanitized':san,'rotation':rot,'width':w,'height':h,'mode':mode,'returncode':run.returncode,'stdout':run.stdout,'stderr':run.stderr});(out/'results.json').write_text(json.dumps(rows,indent=2));run.check_returncode()
files=[ROOT/'Services/scene_host/host.c',ROOT/'lib/PortableApps/include/PortableSafeViewport.h',ROOT/'test/scene/safe_viewport_test.c',Path(__file__),ROOT/'scripts/scene_sdk.py']
(out/'source.json').write_text(json.dumps({str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},indent=2));print(len(rows),'safe-viewport provider cases passed')
