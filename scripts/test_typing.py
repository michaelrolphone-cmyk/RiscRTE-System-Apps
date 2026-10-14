#!/usr/bin/env python3
"""T046 actual shared owners: 60 WPM / 90 WPM burst at current geometries."""
import argparse,os,subprocess,json,hashlib
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc=stage_sdk(a.runtime.resolve(),ROOT,out/'sdk');flags=['-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(inc),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'Services/text_input')]
if a.sanitize:flags+=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
cc=os.environ.get('CC','cc');obj=out/'text.o';subprocess.run([cc,*flags,'-Dt5_driver_get=typing_text_driver_get','-c',str(ROOT/'Services/text_input/host.c'),'-o',str(obj)],check=True)
exe=out/'typing';subprocess.run([cc,*flags,str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/text_input/typing_test.c'),str(obj),'-o',str(exe)],check=True)
results=[]
for profile in ['watch','wave','x4']:
 for period in [200,133]:
  for repeat in [0,1]:
   hashes=[]
   for delay in [0,16,120,2300]:
    raster=out/f'{profile}-{period}-{repeat}-{delay}.frame'
    r=subprocess.run([str(exe),profile,str(period),str(delay),str(repeat),'type',str(raster)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=120)
    if r.returncode:print(r.stderr);r.check_returncode()
    print(r.stdout,end='');hashes.append(hashlib.sha256(raster.read_bytes()).hexdigest());results.append(r.stdout.strip())
   assert len(set(hashes))==1,'Final draft render depends on refresh latency'
 subprocess.run([str(exe),profile,'200','16','0','style',str(out/f'{profile}-style.frame')],check=True)
(out/'results.json').write_text(json.dumps({'cases':results,'sanitized':a.sanitize,'hardware_tested':False,'characters_each':200,'touch_duration_ms':20,'wpm_convention':'5 characters per word; 200 ms = 60 WPM, 133 ms >=90 WPM','sources':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Services/scene_host/host.c',ROOT/'Services/scene_host/keyboard.inc',ROOT/'Services/text_input/host.c',ROOT/'test/text_input/typing_test.c']}},indent=2)+'\n')
