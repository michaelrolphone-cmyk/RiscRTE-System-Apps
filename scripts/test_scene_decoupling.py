#!/usr/bin/env python3
"""Qualify reconstructed scene logical cadence and baseline raster equality.

This is an edge-stream test, not the missing installed-GT911 physical proof.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,baseline,out,sanitize):
 out.mkdir(parents=True,exist_ok=True);inc=stage_sdk(runtime,ROOT,out/'sdk')
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function']
 if sanitize:flags+=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'};commands=[];results=[]
 def command(args):
  commands.append(list(map(str,args)));r=subprocess.run(args,capture_output=True,text=True,env=env)
  (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:
   (out/'failure.json').write_text(json.dumps({'command':commands[-1],'code':r.returncode,'stdout':r.stdout,'stderr':r.stderr},indent=2)+'\n')
   raise RuntimeError(r.stderr)
  return r.stdout
 executables={}
 for name,source in [('candidate',ROOT),('baseline',baseline)]:
  exe=out/name;command([os.environ.get('CC','cc'),*flags,'-I'+str(inc),str(source/'Services/scene_host/host.c'),str(ROOT/'test/scene/decoupling_test.c'),'-o',str(exe)]);executables[name]=exe
 for fmt in range(1,6):
  for rotation in [0,90,180,270]:
   rasters=[]
   for name,exe in executables.items():
    p=out/f'{name}-{fmt}-{rotation}.bin';command([str(exe),'raster',str(fmt),str(rotation),str(p)]);rasters.append(p.read_bytes())
   assert rasters[0]==rasters[1],(fmt,rotation)
   results.append({'kind':'raster','format':fmt,'rotation':rotation,'sha256':hashlib.sha256(rasters[0]).hexdigest()})
 for layer in range(4):
  rasters=[]
  for name,exe in executables.items():
   p=out/f'{name}-layer{layer}.bin';command([str(exe),'raster','1','270',str(p),str(layer)]);rasters.append(p.read_bytes())
  assert rasters[0]==rasters[1],layer
  results.append({'kind':'nova7-layer','layer':layer,'sha256':hashlib.sha256(rasters[0]).hexdigest()})
 for fmt in [1,3,5]:
  command([str(executables['candidate']),'overlap',str(fmt),'270']);results.append({'kind':'overlap-layer-gap-home','format':fmt})
  for period,count in [(200,600),(133,120),(80,120),(60,120)]:
   expected=None
   for delay in [0,16,120,2300]:
    output=command([str(executables['candidate']),'cadence',str(fmt),'270',str(period),str(count),str(delay)]).strip()
    digest=output.split('digest=')[1].split()[0]
    if expected is None:expected=digest
    assert expected==digest
    results.append({'kind':'logical-cadence','format':fmt,'period_ms':period,'count':count,'panel_ms':delay,'output':output})
 proof={'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Services/scene_host/host.c',ROOT/'Services/scene_host/keyboard_paper.inc',ROOT/'test/scene/decoupling_test.c']},'sanitized':sanitize,'cases':results,'limits':['Physical edges supplied to provider fixture; exact installed GT911 source unavailable.','No measured device latency or hardware qualification.','Logical cadence equality is independent of presentation; CPU/transport physical cadence remains a separate qualification.']}
 (out/'qualification.json').write_text(json.dumps(proof,indent=2)+'\n');print(f'{len(results)} source-bound cadence/raster cases passed; physical proof remains open.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.runtime.resolve(),a.baseline.resolve(),a.output.resolve(),a.sanitize)
