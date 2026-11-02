#!/usr/bin/env python3
"""Shared retained cycle normal/sanitized boundary tests; no hardware."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--utilities',type=Path,required=True)
p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();inc=out/'include'
inc.mkdir(parents=True,exist_ok=True)
shutil.copytree(root/'lib/PortableApps/include',inc,dirs_exist_ok=True)
for group in ('app','driver','hardware'):
 for src in (a.runtime/'sdk'/group).glob('*.h'):shutil.copyfile(src,inc/src.name)
for src in (a.utilities/'lib/Alarm/include').glob('*.h'):shutil.copyfile(src,inc/src.name)
shutil.copytree(root/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
runs=[]
for mode in ('normal','sanitized'):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
 binary=out/mode
 command=['cc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror',*flags,'-DPORTABLE_DESK_POINTS_FACE','-DPORTABLE_DESK_POINTS_SNAPSHOT','-I'+str(inc),str(root/'test/native_apps/home_retained_clock_test.c'),'-o',str(binary)]
 subprocess.run(command,check=True)
 result=subprocess.run([str(binary)],check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 print(result.stdout,end='');runs.append({'mode':mode,'command':command,'output':result.stdout})
files=[root/'lib/PortableApps/include/PortableHomeRetainedClock.h',root/'lib/PortableApps/include/PortableDeskClock.h',root/'test/native_apps/home_retained_clock_test.c',Path(__file__)]
(out/'receipt.json').write_text(json.dumps({'runs':runs,'hardware_tested':False,'sha256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
