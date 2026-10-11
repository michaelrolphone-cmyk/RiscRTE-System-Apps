#!/usr/bin/env python3
"""Qualify actual adapter intent independently of transition rendering."""
import argparse, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
inc=a.output/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',a.output/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):shutil.copyfile(a.runtime_sdk/name,inc/name)
runs=[]
for sanitize in (False,True):
 for transitions in (False,True):
  name=f'intent-{int(sanitize)}-{int(transitions)}';binary=a.output/name
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
  if transitions:flags+=['-DPORTABLE_PAPER_TRANSITIONS']
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/interactive_paper_intent_test.c'),'-Wl,--wrap=free','-o',str(binary)]
  subprocess.run(cmd,check=True)
  result=subprocess.run([binary],check=True,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=20)
  print(name,result.stdout.strip(),flush=True);runs.append(dict(sanitize=sanitize,transitions=transitions,result=result.stdout.strip()))
(a.output/'evidence.json').write_text(json.dumps(dict(runs=runs,hardware_verified=False),indent=2)+'\n')
