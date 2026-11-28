#!/usr/bin/env python3
"""Focused copied-preference pending-frame barrier; injected services, not hardware."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 fixture=ROOT/'test/native_apps/quick_pending_frame_test.c'
 flags=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(ROOT/'lib/PortableApps/include')]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 dep=subprocess.check_output([*flags,'-MM',str(fixture)],text=True)
 files=[Path(x).resolve() for x in dep.replace('\\\n',' ').split(':',1)[1].split()]+[Path(__file__).resolve()]
 hashes={str(x):digest(x) for x in files};exe=out/'test';cmd=[*flags,str(fixture),'-o',str(exe)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr);r.check_returncode()
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
 (out/'test.log').write_text(r.stdout+r.stderr);r.check_returncode()
 assert hashes=={str(x):digest(x) for x in files},'Consumed source changed during test'
 receipt={'passed':True,'sanitized':a.sanitize,'command':cmd,'source_sha256':hashes,'executable_sha256':digest(exe),'output':r.stdout,'scope':'Owning pure-C barrier with injected services; not Runtime, target or device qualification.'}
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout,end='')
if __name__=='__main__':main()
