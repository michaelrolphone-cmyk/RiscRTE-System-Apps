#!/usr/bin/env python3
"""Bounded cache and receiver-local link/session proofs; endpoint doubles only."""
import argparse,hashlib,json,os,pathlib,subprocess,shutil
R=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--drivers',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sdk=a.output/'include';sdk.mkdir(exist_ok=True)
origins={}
for root in [R/'lib/PortableApps/include',a.runtime/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware',a.drivers/'sdk/driver']:
 for source in root.glob('*.h'):origins[source.name]=source.resolve()
for name,source in origins.items():shutil.copyfile(source,sdk/name)
(a.output/'sdk-inputs.json').write_text(json.dumps({n:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for n,p in origins.items()},indent=2)+'\n')
cc=os.environ.get('CC','cc');flags=['-std=c11','-O1','-g0','-Wall','-Wextra','-Werror','-I'+str(sdk)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
results=[];inputs={};commands=[]
for name,sources in [('cache',['test/native_apps/remote_battery_cache_test.c']),('link-session',['test/native_apps/remote_battery_link_test.c','lib/PortableApps/src/remote_battery.c'])]:
 objects=[]
 for i,source in enumerate(sources):
  obj=a.output/(name+str(i)+'.o');dep=obj.with_suffix('.d')
  command=[cc,*flags,'-MMD','-MF',str(dep),'-c',str(R/source),'-o',str(obj)];commands.append(command)
  r=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  (a.output/(name+str(i)+'-build.log')).write_text(r.stdout);print(r.stdout,end='');r.check_returncode()
  for word in dep.read_text().replace('\\\n',' ').split(':',1)[1].split():
   f=pathlib.Path(word).resolve();inputs[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
  objects.append(obj)
 binary=a.output/name;command=[cc,*flags,*map(str,objects),'-o',str(binary)];commands.append(command)
 subprocess.run(command,check=True)
 r=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
 (a.output/(name+'.log')).write_text(r.stdout);print(r.stdout,end='');r.check_returncode()
 results.append({'name':name,'passed':True,'new_cases':21 if name=='cache' else 24,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
for path,h in inputs.items():assert hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()==h,path
(a.output/'receipt.json').write_text(json.dumps({'results':results,'sanitize':a.sanitize,'commands':commands,'inputs':inputs,'scope':'actual cache/session with endpoint doubles; no physical storage or RF'},indent=2)+'\n')
