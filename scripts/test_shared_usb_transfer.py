#!/usr/bin/env python3
"""Compile and execute the actual common USB controller + scene loop."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--msc-sdk',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
results=[]
for san in (False,True):
 target=a.output/('host-'+str(int(san)))
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(a.msc_sdk),
 '-I'+str(ROOT/'sdk/usb'),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'lib/PortableApps/include'),
 '-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/shared_usb_transfer_test.c'),'-o',str(target)]
 subprocess.run(cmd,check=True)
 cases=[(mode,'') for mode in range(17)]+[(0,f) for f in ('acquire','release','diagnostic','begin','poll','end','prepare','open','update','configure','snapshot','next','close')]+[(3,'last_error')]+[(7,f) for f in ('alarm_step','alarm_status','alarm_ack')]
 for mode,fault in cases:
  r=subprocess.run([str(target),str(mode)]+([fault] if fault else []),text=True,capture_output=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
  (a.output/f'case-{int(san)}-{mode}-{fault}.log').write_text(r.stdout+r.stderr)
  print(r.stdout+r.stderr,flush=True);r.check_returncode()
  results.append({'sanitized':san,'case':mode,'fault':fault,'passed':True})
(a.output/'results.json').write_text(json.dumps({'cases':results,'physical_testing':False},indent=2))
