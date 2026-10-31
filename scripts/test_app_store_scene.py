#!/usr/bin/env python3
"""Real App Store semantic controller/presenter; Runtime/network/storage endpoints doubled."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
cmd=['cc','-std=c11','-O0','-g0','-pipe','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(ROOT/'Apps'),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/Wifi/include'),'-I'+str(a.runtime/'sdk/app')]
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
cmd+=['-MMD','-MF',str(a.output/'dependencies.d'),str(ROOT/'test/app_store_scene/controller_test.c'),'-o',str(a.output/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);(a.output/'build.log').write_text(r.stdout+r.stderr);r.check_returncode();r=subprocess.run([str(a.output/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(a.output/'test.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
paths=(a.output/'dependencies.d').read_text().replace('\\\n',' ').split(':',1)[1].split()+[str(Path(__file__).resolve())]
(a.output/'receipt.json').write_text(json.dumps({'sanitize':a.sanitize,'command':cmd,'result':r.stdout,'source_sha256':{str(Path(p).resolve()):hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths},'hardware_tested':False},indent=2)+'\n')
