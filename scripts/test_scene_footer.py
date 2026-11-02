#!/usr/bin/env python3
"""Actual presenter/native-navigation fixed-footer occlusion regression."""
import argparse,json,os,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('runtime','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--provider-root',type=Path);p.add_argument('--baseline',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);provider=(a.provider_root or ROOT).resolve()
include=stage_sdk(a.runtime.resolve(),provider,a.output/'sdk')
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(include),'-I'+str(ROOT/'test/scene'),'-DSCENE_HOST_WATCH_240=0','-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_SOURCE="'+str(provider/'Services/scene_host/host.c')+'"']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
exe=a.output/'test'
subprocess.run(['cc',*flags,str(ROOT/'test/scene/footer_probe.c'),str(ROOT/'test/scene/footer_test.c'),'-o',str(exe)],check=True)
results=[]
for w,h in ((160,160),(240,240),(410,502),(320,180),(502,410)):
 r=subprocess.run([str(exe),str(w),str(h)],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
 (a.output/(str(w)+'x'+str(h)+'.log')).write_text(r.stdout+r.stderr)
 if a.baseline:assert r.returncode==-6 and 'scene_footer_target' in r.stderr,(w,h,r.returncode,r.stderr)
 else:assert r.returncode==0,(w,h,r.returncode,r.stderr)
 results.append({'width':w,'height':h,'returncode':r.returncode,'expected_failure':a.baseline})
(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print('fixed footer',len(results),'profiles matched expected results')
