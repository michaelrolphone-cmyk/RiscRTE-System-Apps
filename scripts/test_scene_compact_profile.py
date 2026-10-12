#!/usr/bin/env python3
"""Qualify compact API discovery/fail-closed behavior and unchanged legacy rasters."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
from build_scene_services import HOST_PROFILE_FLAGS,host_manifest
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,output):
 output.mkdir(parents=True,exist_ok=True);inc=stage_sdk(runtime,ROOT,output/'sdk')
 cc=os.environ.get('CC','cc');flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie','-I'+str(inc)]
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
 def build(test,name,profile):
  exe=output/name;subprocess.run([cc,*flags,*HOST_PROFILE_FLAGS[profile],str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene'/test),'-o',str(exe)],check=True);return exe
 compact=host_manifest('watch-compact');full=host_manifest('full')
 assert compact['version']=='0.4.3' and full['version']=='0.4.2'
 assert compact['requires']==[x for x in full['requires'] if x['capability']!='input.navigation']
 subprocess.run([str(build('compact_profile_test.c','compact-api','watch-compact'))],env=env,check=True)
 hashes={}
 for test,mode in [('host_test.c','behavior'),('keyboard_test.c','keyboard-raster'),('watch_pixels_test.c','watch-pixels')]:
  for profile in HOST_PROFILE_FLAGS:
   exe=build(test,mode+'-'+profile,profile)
   for panel in ('watch',):
    frame=output/(mode+'-'+profile+'-'+panel+'.frame')
    subprocess.run([str(exe),str(frame)] if mode=='watch-pixels' else [str(exe),mode,panel,str(frame)],env=env,check=True)
    hashes[frame.name]=hashlib.sha256(frame.read_bytes()).hexdigest()
  for panel in ('watch',):
   assert hashes[mode+'-full-'+panel+'.frame']==hashes[mode+'-watch-compact-'+panel+'.frame']
 (output/'raster-parity.json').write_text(json.dumps(hashes,indent=2)+'\n')
 print('Compact profile API rejection and full/compact legacy scene + keyboard raster byte parity PASS')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();run(a.runtime.resolve(),a.output.resolve())
