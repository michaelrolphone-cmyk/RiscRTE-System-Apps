#!/usr/bin/env python3
"""Strict isolated inventory/source identity and transaction-custody tests."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-Wall','-Wextra','-Werror','-O0','-g0','-pipe','-I'+str(a.runtime.resolve()/'sdk/app'),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'Apps')]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 sources=[ROOT/'Apps/app_store_inventory.c',ROOT/'test/app_store_inventory/controller_test.c'];objects=[];inputs=set()
 for i,src in enumerate(sources):
  obj=out/f'{i}.o';dep=out/f'{i}.d';cmd=[os.environ.get('CC','cc'),*flags,'-MMD','-MF',str(dep),'-c',str(src),'-o',str(obj)]
  r=subprocess.run(cmd,capture_output=True,text=True);(out/f'{i}-build.log').write_text(r.stdout+r.stderr);r.check_returncode();objects.append(obj);inputs.update(dep.read_text().replace('\\\n',' ').split(':',1)[1].split())
 hashes={str(Path(x).resolve()):hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in inputs};exe=out/'inventory-test'
 r=subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,objects),'-o',str(exe)],capture_output=True,text=True);(out/'link.log').write_text(r.stdout+r.stderr);r.check_returncode()
 r=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(out/'test.log').write_text(r.stdout+r.stderr);r.check_returncode()
 assert all(hashlib.sha256(Path(x).read_bytes()).hexdigest()==h for x,h in hashes.items())
 hashes[str(Path(__file__).resolve())]=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
 (out/'receipt.json').write_text(json.dumps({'sanitized':a.sanitize,'result':r.stdout.strip(),'inputs':hashes,'scope':'Production app-local installed inventory facade; Runtime/catalog/update/volume endpoints are deterministic doubles, not native storage or network execution.'},indent=2)+'\n');print(r.stdout)
if __name__=='__main__':main()
