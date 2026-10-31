#!/usr/bin/env python3
"""Real App Store semantic controller/presenter; Runtime/network/storage endpoints doubled."""
import argparse,hashlib,json,os,subprocess,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
inc=a.output/'include';inc.mkdir(exist_ok=True)
origin=json.loads((ROOT/'test/app_store_background/source-origin.json').read_text())
for name,digest in origin['blobs'].items():
 data=(a.utilities/name).read_bytes();assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==digest
headers={}
for folder in [a.runtime/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware',ROOT/'sdk/app',ROOT/'lib/PortableApps/include',ROOT/'lib/NativeApps/include',ROOT/'lib/Wifi/include',a.utilities/'lib/Bluetooth/include']:
 for src in folder.glob('*.h'):headers.setdefault(src.name,src.resolve())
for name in ('battery_broadcast.h','battery_preference_adapter.h'):headers[name]=(a.utilities/'Apps'/name).resolve()
for name,src in headers.items():shutil.copyfile(src,inc/name)
cmd=['cc','-std=c11','-O0','-g0','-pipe','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(ROOT/'Apps'),'-I'+str(inc)]
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
before={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for folder in (ROOT/'Apps',ROOT/'sdk',ROOT/'test/app_store_scene',inc) for p in folder.rglob('*') if p.is_file()}
inputs=set();objects=[];commands=[]
for i,src in enumerate([ROOT/'test/app_store_scene/main_loop_test.c',ROOT/'Apps/app_store_inventory.c']):
 obj=a.output/f'{i}.o';dep=a.output/f'{i}.d';command=[*cmd,'-MMD','-MF',str(dep),'-c',str(src),'-o',str(obj)]
 r=subprocess.run(command,capture_output=True,text=True);(a.output/f'{i}-build.log').write_text(r.stdout+r.stderr);r.check_returncode();commands.append(command);objects.append(obj);inputs.update(dep.read_text().replace('\\\n',' ').split(':',1)[1].split())
command=[*cmd,*map(str,objects),'-o',str(a.output/'test')];r=subprocess.run(command,capture_output=True,text=True);(a.output/'link.log').write_text(r.stdout+r.stderr);r.check_returncode();commands.append(command)
results=[]
for case in range(11):
 r=subprocess.run([str(a.output/'test'),str(case)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(a.output/f'test-{case}.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode();results.append(r.stdout)
paths=sorted(inputs)+[str(Path(__file__).resolve())]
for p in inputs:
 assert before[str(Path(p).resolve())]==hashlib.sha256(Path(p).read_bytes()).hexdigest(),('source changed',p)
(a.output/'receipt.json').write_text(json.dumps({'sanitize':a.sanitize,'commands':commands,'result':results,'source_sha256':{str(Path(p).resolve()):hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths},'hardware_tested':False,'scope':'Actual production app_main, controller, presenter and inventory; scene/update/Runtime endpoints doubled, background capability clean-absent. No actual renderer, resident host, RF or native update transaction proof.','published_utilities':origin,'header_origins':{name:{'path':str(src),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()} for name,src in headers.items()}},indent=2)+'\n')
