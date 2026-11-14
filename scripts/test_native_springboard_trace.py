#!/usr/bin/env python3
"""Actual native Watch launcher pixels/actions: trace off, null and enabled."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[1];rows=[];baseline={}
with tempfile.TemporaryDirectory(prefix='native-launch-trace-',dir=a.output) as tmp:
 t=Path(tmp);inc=t/'include';shutil.copytree(r/'lib/PortableApps/include',inc);shutil.copytree(r/'lib/PortableApps/time',t/'time')
 for n in ['RiscRuntimeV1.h','RiscPerformanceV1.h','RiscRealtimeV1.h']:shutil.copyfile(a.runtime_sdk/n,inc/n)
 s=(r/'test/native_apps/springboard_nova_test.c').read_text().replace('"../../Apps/SpringboardPresentation.h"','"'+str(r/'Apps/SpringboardPresentation.h')+'"')
 s=s.replace('static void save_frame(void){','static void save_frame(void){\n uint64_t h=1469598103934665603ull;for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);++i){h^=pixels[i];h*=1099511628211ull;}printf("FRAME %016llx\\n",(unsigned long long)h);')
 s=s.replace('static const risc_runtime_api_v1 rt=', '''#ifdef TEST_TRACE_CALLBACK
#include "RiscPerformanceV1.h"
static unsigned trace_count,trace_id;
static uint32_t test_trace(uint32_t id,uint32_t phase,uint32_t value){(void)value;assert(++trace_count<10000);if(phase==RISC_PERF_INTERACTION_BEGIN)++trace_id;return id?id:trace_id;}
#endif
static const risc_runtime_api_v1 rt=''').replace('.release=release\n','.release=release\n#ifdef TEST_TRACE_CALLBACK\n ,.trace=test_trace\n#endif\n')
 s=s.replace('int main(int argc,char **argv){','static int fixture_main(int argc,char **argv){')
 s=s.rstrip()[:-1]+'return 0;}\n'
 s+='''\nint main(int argc,char **argv){int result=fixture_main(argc,argv);
#ifdef TEST_TRACE_CALLBACK
assert(trace_count>0);fprintf(stderr,"TRACE records=%u\\n",trace_count);
#endif
return result;}\n'''
 src=t/'fixture.c';src.write_text(s)
 helpers=[str(r/'lib/PortableApps/src'/n) for n in ['PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']]
 for san in [False,True]:
  for mode in ['off','null','enabled']:
   f=['-std=c11','-O1','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY','-DTEST_UNKNOWN_REFRESH','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_TIME','-DTEST_WIDTH=466','-DTEST_HEIGHT=466']
   if mode!='off':f+=['-DPORTABLE_PERFORMANCE_TRACE']
   if mode=='enabled':f+=['-DTEST_TRACE_CALLBACK']
   if san:f+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
   exe=t/'test';subprocess.run(['cc',*f,'-I'+str(inc),'-I'+str(r/'lib/NativeApps/include'),str(r/'Apps/springboard.c'),str(r/'lib/PortableApps/src/adapter.c'),str(src),*helpers,'-Wl,--wrap=t5_app_get_api','-o',str(exe)],check=True)
   for case in [0,1,2,6,14,15,16,23,24,36,37,40,41,42]:
    start=time.perf_counter_ns();v=subprocess.run([exe,str(case)],check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});elapsed=time.perf_counter_ns()-start
    if case not in baseline:baseline[case]=v.stdout
    assert baseline[case]==v.stdout,(case,mode,san)
    rows.append(dict(case=case,mode=mode,sanitized=san,stdout_sha256=hashlib.sha256(v.stdout.encode()).hexdigest(),trace=v.stderr.strip(),host_process_ns=elapsed))
   print(mode,san,'14 cases PASS',flush=True);exe.unlink()
(a.output/'springboard-trace-parity.json').write_text(json.dumps(rows,indent=2)+'\n')
