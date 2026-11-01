#!/usr/bin/env python3
"""Actual sparse optional-acquisition guard and extracted KV result contract."""
import argparse,pathlib,subprocess,hashlib,json,os,re
R=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
defs=['PORTABLE_QUICK_REMOTE_BATTERY','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_RESIDENT_SHELL_HOST','PORTABLE_PAPER_TRANSITIONS','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_ALARM_CLIENT','PORTABLE_ALARM_TERMINAL_RETENTION','ALARM_SERVICE_TAGGED_V2','PORTABLE_DESK_CLOCK','PORTABLE_DESK_CLOCK_SPARSE_START','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_SLEEP_MANUAL_ONLY']
sources=[R/'test/native_apps/quick_remote_sparse_test.c']+[R/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c','remote_battery.c']]
sparse=R/'lib/PortableApps/src/sparse_clock_adapter.inc';text=sparse.read_text()
body=re.search(r'static int32_t desk_kv_result\(int32_t rc\) \{.*?\n\}',text,re.S).group()
kv=a.output/'kv-result.c';kv.write_text('#include "RiscKeyValueV1.h"\n#include <assert.h>\n#include <stdio.h>\nstatic unsigned retained;static void portable_desk_adapter_retain(void){retained++;}\n'+body+'\nint main(void){int ok[]={0,-1,-2,-3,-5,-6};for(unsigned i=0;i<6;i++){retained=0;assert(desk_kv_result(ok[i])==ok[i]&&!retained);}assert(desk_kv_result(-4)==-4&&retained==1);assert(desk_kv_result(999)==999&&retained==2);puts("actual KV result: 8 status cases PASS");}\n')
inputs={str(sparse):hashlib.sha256(sparse.read_bytes()).hexdigest()};results=[];commands=[]
for san in [False,True]:
 label='sanitized' if san else 'normal';flags=['-std=c11','-O1','-g0','-Wall','-Wextra','-Werror','-Wno-unused-function','-ffunction-sections','-fdata-sections',*['-D'+d for d in defs],'-I'+str(a.sdk.resolve()),'-I'+str(R/'lib/NativeApps/include')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 for name,units,cases in [('sparse',sources,10),('kv-result',[kv],8)]:
  objects=[]
  for i,source in enumerate(units):
   obj=a.output/(label+name+str(i)+'.o');dep=obj.with_suffix('.d');cmd=['cc',*flags,'-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)];commands.append(cmd)
   r=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(a.output/(label+name+str(i)+'.log')).write_text(r.stdout);print(r.stdout,end='',flush=True);r.check_returncode()
   for word in dep.read_text().replace('\\\n',' ').split(':',1)[1].split():
    f=pathlib.Path(word).resolve();inputs[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
   objects.append(str(obj))
  binary=a.output/(label+'-'+name);cmd=['cc',*flags,'-Wl,--gc-sections',*objects,'-o',str(binary)];commands.append(cmd);subprocess.run(cmd,check=True)
  r=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));(a.output/(label+'-'+name+'.log')).write_text(r.stdout);print(r.stdout,end='',flush=True);r.check_returncode()
  results.append({'name':name,'cases':cases,'sanitize':san,'passed':True,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
for path,h in inputs.items():assert hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()==h,path
(a.output/'receipt.json').write_text(json.dumps({'results':results,'commands':commands,'inputs':inputs,'scope':'actual sparse guard with endpoint doubles plus exact extracted KV status function; no Runtime or hardware'},indent=2)+'\n')
