#!/usr/bin/env python3
"""Selected native Settings render/controller parity with bounded RAM trace hooks."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
rows=[]
with tempfile.TemporaryDirectory(prefix='native-trace-',dir=a.output) as tmp:
 t=Path(tmp);inc=t/'include';shutil.copytree(r/'lib/PortableApps/include',inc)
 shutil.copytree(r/'lib/PortableApps/time',t/'time')
 for n in ['RiscRuntimeV1.h','RiscPerformanceV1.h']:shutil.copyfile(a.runtime_sdk/n,inc/n)
 for n in ['settings_native_geometry_test.c','settings_sleep_defaults_test.c','portable_settings_test.c']:
  s=(r/'test/native_apps'/n).read_text().replace('"../../lib/PortableApps/src/adapter.c"','"'+str(r/'lib/PortableApps/src/adapter.c')+'"')
  if n=='portable_settings_test.c':
   marker='static const risc_runtime_api_v1 runtime_api='
   s=s.replace(marker,'''#ifdef TEST_TRACE_CALLBACK
static unsigned test_trace_count,test_trace_id;
static uint32_t test_trace(uint32_t id,uint32_t phase,uint32_t value){
 (void)value;assert(!failed);assert(++test_trace_count<10000);
 if(phase==RISC_PERF_INTERACTION_BEGIN)test_trace_id++;
 return id?id:test_trace_id;
}
#endif
'''+marker).replace('.release=test_release};','.release=test_release\n#ifdef TEST_TRACE_CALLBACK\n,.trace=test_trace\n#endif\n};')
  if n=='settings_native_geometry_test.c':
   start=s.index(' const char*dir=getenv("SETTINGS_NATIVE_FRAMES")');end=s.index('\n}',start)
   s=s[:start]+''' uint64_t h=1469598103934665603ull;
 for(unsigned i=0;i<sizeof(framebuffer)/sizeof(*framebuffer);++i){h^=framebuffer[i];h*=1099511628211ull;}
 printf("FRAME %s %016llx\\n",name,(unsigned long long)h);'''+s[end:]
   s=s.replace(' puts("PASS native Settings', '#ifdef TEST_TRACE_CALLBACK\n assert(test_trace_count>0);fprintf(stderr,"TRACE records=%u\\n",test_trace_count);\n#endif\n puts("PASS native Settings')
  (t/n).write_text(s)
 base=None
 for sanitized in [False,True]:
  for mode in ['off','null','enabled']:
   exe=t/'test';flags=['-std=c11','-O1','-Wall','-Wextra','-Werror','-DPORTABLE_SETTINGS_NATIVE_GEOMETRY','-DPORTABLE_SAFE_VIEWPORT','-DTEST_SETTINGS_RASTER_WIDTH=466','-DTEST_SETTINGS_RASTER_HEIGHT=466','-DTEST_SETTINGS_INSET=69']
   if mode!='off':flags+=['-DPORTABLE_PERFORMANCE_TRACE']
   if mode=='enabled':flags+=['-DTEST_TRACE_CALLBACK']
   if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
   cmd=['cc',*flags,'-I'+str(inc),'-I'+str(r/'lib/NativeApps/include'),'-I'+str(r/'test/native_apps'),str(r/'Apps/settings.c'),str(t/'settings_native_geometry_test.c'),'-o',str(exe)]
   subprocess.run(cmd,check=True)
   v=subprocess.run([exe],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   if base is None:base=v.stdout
   assert v.stdout==base,(mode,sanitized)
   rows.append(dict(mode=mode,sanitized=sanitized,stdout_sha256=hashlib.sha256(v.stdout.encode()).hexdigest(),detail=v.stderr.strip()))
   print(rows[-1],flush=True);exe.unlink()
(a.output/'selected-trace-parity.json').write_text(json.dumps(rows,indent=2)+'\n')
