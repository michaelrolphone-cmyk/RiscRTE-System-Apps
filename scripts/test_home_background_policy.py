#!/usr/bin/env python3
"""Exercise the two production Home policy clients and their storage guards."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime-sdk',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();out=a.output_dir.resolve();inc=out/'include'
shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True)
shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):
 shutil.copyfile(a.runtime_sdk/name,inc/name)
fixture=ROOT/'test/native_apps/home_background_policy_test.c'
runs=[]
for sanitized in (False,True):
 for legacy in (True,False):
  binary=out/f'policy-{int(legacy)}-{int(sanitized)}'
  flags=['-DTEST_LEGACY_STORAGE_READ'] if legacy else []
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-I'+str(inc),str(fixture),'-o',str(binary)],check=True)
  result=subprocess.run([binary],check=True,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=20)
  print(result.stdout.strip());runs.append({'legacy_read_dispatch':legacy,'sanitized':sanitized,'result':result.stdout.strip()})
files=[fixture,Path(__file__).resolve(),*[ROOT/'lib/PortableApps/src'/n for n in ('adapter.c','contexts_adapter.inc','broadcast_adapter.inc','sparse_clock_adapter.inc','native_custody_adapter.inc')],*[ROOT/'lib/PortableApps/include'/n for n in ('PortableContextsClient.h','PortableBroadcastClient.h')]]
(out/'evidence.json').write_text(json.dumps({'hardware_tested':False,'runs':runs,'sources':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
