#!/usr/bin/env python3
"""Read-only BLE foreground adapter, pinned published controller provenance."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();inc=out/'include';inc.mkdir(parents=True,exist_ok=True)
 origin=json.loads((ROOT/'test/app_store_background/source-origin.json').read_text())
 for name,expected in origin['blobs'].items():
  data=(a.utilities/name).read_bytes();assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==expected,name
 files={}
 for folder in [a.runtime/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware',ROOT/'lib/PortableApps/include',ROOT/'lib/NativeApps/include',a.utilities/'lib/Bluetooth/include']:
  for src in folder.glob('*.h'):files.setdefault(src.name,src.resolve())
 for name in ('battery_broadcast.h','battery_preference_adapter.h'):files[name]=(a.utilities/'Apps'/name).resolve()
 for name,src in files.items():shutil.copyfile(src,inc/name)
 flags=['-std=c11','-Wall','-Wextra','-Werror','-O0','-g0','-pipe','-I'+str(inc),'-I'+str(ROOT/'Apps')]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 src=ROOT/'test/app_store_background/controller_test.c';dep=out/'test.d';exe=out/'background-test'
 r=subprocess.run([os.environ.get('CC','cc'),*flags,'-MMD','-MF',str(dep),str(src),'-o',str(exe)],capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr);r.check_returncode()
 inputs=dep.read_text().replace('\\\n',' ').split(':',1)[1].split();hashes={str(Path(x).resolve()):hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in inputs}
 r=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(out/'test.log').write_text(r.stdout+r.stderr);r.check_returncode()
 assert all(hashlib.sha256(Path(x).read_bytes()).hexdigest()==h for x,h in hashes.items())
 origins={str(files[Path(x).name]):hashlib.sha256(files[Path(x).name].read_bytes()).hexdigest() for x in inputs if Path(x).parent==inc}
 (out/'receipt.json').write_text(json.dumps({'sanitized':a.sanitize,'result':r.stdout.strip(),'inputs':hashes,'origins':origins,'published_utilities':origin,'scope':'Production wrapper and published Battery helper; deterministic Runtime/KV/BLE service endpoints, not native RF execution.'},indent=2)+'\n');print(r.stdout)
if __name__=='__main__':main()
