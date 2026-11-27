#!/usr/bin/env python3
"""Native navigation regression: scrolling RGB headers must become actionable."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--provider-root',type=Path);p.add_argument('--baseline',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);provider=(a.provider_root or ROOT).resolve();include=stage_sdk(a.runtime.resolve(),provider,out)
flags=['-std=c11','-O0','-g1','-pipe','-Wall','-Wextra','-Werror','-I'+str(include)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
units=[provider/'Services/scene_host/host.c',ROOT/'test/header_focus/header_focus_test.c'];commands=[]
def execute(cmd,name,check=True):
 commands.append(cmd);(out/'commands.json').write_text(json.dumps(commands,indent=2));r=subprocess.run(cmd,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(name+'.log')).write_text(r.stdout+r.stderr)
 if check:r.check_returncode()
 return r
inputs={}
for i,u in enumerate(units):
 raw=execute(['cc',*flags,'-MM',str(u)],'deps-'+str(i)).stdout
 for item in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]):
  path=Path(item).resolve();inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
for path in [Path(__file__).resolve(),ROOT/'scripts/scene_sdk.py']:inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
execute(['cc',*flags,*map(str,units),'-o',str(out/'test')],'build')
results=[]
for profile in ['watch','wave','square','paper']:
 r=execute([str(out/'test'),profile],profile,False)
 expected_failure=a.baseline and profile!='paper'
 assert (r.returncode==-6 and 'wrapped_header=1 result=1' in r.stderr) if expected_failure else r.returncode==0,(profile,r.returncode,r.stderr)
 results.append({'profile':profile,'returncode':r.returncode,'expected_baseline_failure':expected_failure})
assert inputs=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs},'source changed during test'
(out/'receipt.json').write_text(json.dumps({'baseline':a.baseline,'sanitize':a.sanitize,'results':results,'inputs':inputs,'binary_sha256':hashlib.sha256((out/'test').read_bytes()).hexdigest(),'scope':'Actual scene host, provider raster and native-navigation endpoint fixture. Not actual Runtime or physical display/input.'},indent=2)+'\n');print(len(results),'header-focus profiles matched expected outcomes')
