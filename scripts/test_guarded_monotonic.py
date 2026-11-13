#!/usr/bin/env python3
"""Execute exact extracted production wrapper bodies and copied-table slices."""
import argparse,pathlib,subprocess,os,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
def function(text,name):
 start=text.index(('static uint32_t ' if name=='millis_now' else 'static bool ')+name+'(');brace=text.index('{',start);depth=1;i=brace+1
 while depth:
  depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
paths=[root/'lib/PortableApps/src'/n for n in ('native_custody_adapter.inc','sparse_clock_adapter.inc','adapter.c')];cust,desk,adapter=[p.read_text() for p in paths]
bodies='\n'.join([function(cust,n) for n in ('custody_monotonic','custody_health')]+[function(desk,n) for n in ('desk_guard_monotonic','desk_guard_health')])
slices=[]
for dest in ('desk_guarded_runtime','settings_runtime'):
 start=adapter.index('  memset(&'+dest+',0,sizeof('+dest+'));');end=adapter.index('.request_launch=',start);end=adapter.index(';',end)+1;slices.append(adapter[start:end])
# Both Settings copied-table sites must remain identical.
assert adapter.count(slices[1])==2
h=a.output/'production_slices.inc';h.write_text(bodies+'\n'+function(adapter,'millis_now')+'\nstatic void copy_desk(void){\n'+slices[0]+'\n}\nstatic void copy_custody(void){\n'+slices[1]+'\n}\n')
rows=[]
for san in (False,True):
 for setup in (False,True):
  target=a.output/f'test-{int(san)}-{int(setup)}';flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*(['-DPORTABLE_FILE_SETUP'] if setup else []),'-I'+str(a.sdk),'-I'+str(a.output),str(root/'test/native_apps/guarded_monotonic_test.c'),'-o',str(target)];subprocess.run(cmd,check=True);r=subprocess.run([target],check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(r.stdout,end='');rows.append({'sanitized':san,'file_setup':setup,'command':cmd,'stdout':r.stdout})
inputs=[*paths,root/'test/native_apps/guarded_monotonic_test.c',pathlib.Path(__file__),a.sdk/'RiscRuntimeV1.h',a.sdk/'RiscRuntimeClockV1.h',h]
(a.output/'evidence.json').write_text(json.dumps({'passed':True,'scope':'Exact extracted production functions and copied-table initializer slices; not full adapter integration','runs':rows,'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2)+'\n')
