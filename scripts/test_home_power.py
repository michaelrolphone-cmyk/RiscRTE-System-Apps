#!/usr/bin/env python3
"""Existing generic power capability boundaries; no board or hardware claim."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--watch',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);runs=[]
for mode in ('normal','sanitized'):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
 binary=out/mode;command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'sdk/app'),'-I'+str(a.watch/'sdk/driver'),str(root/'test/native_apps/home_power_test.c'),'-o',str(binary)]
 subprocess.run(command,check=True);result=subprocess.run([str(binary)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 (out/(mode+'.log')).write_text(result.stdout+result.stderr)
 if result.returncode:print(result.stdout+result.stderr);result.check_returncode()
 print(result.stdout,end='');runs.append({'command':command,'output':result.stdout})
 files=[root/'lib/PortableApps/include/PortableHomePower.h',root/'test/native_apps/home_power_test.c',Path(__file__),*[a.watch/'sdk/driver'/n for n in ('RiscDisplayPowerV1.h','RiscPowerSleepV1.h','RiscTimedSleepV1.h','RiscDeepSleepV1.h')]]
(out/'receipt.json').write_text(json.dumps({'runs':runs,'hardware_tested':False,'sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
