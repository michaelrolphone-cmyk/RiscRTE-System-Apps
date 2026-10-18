#!/usr/bin/env python3
"""App Store with selected production native Quick/alarm and atomic preferences."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];PIN='48229bd1593a2e24e95bf4537b495ee615990fd3'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--size');p.add_argument('--case',type=int);a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
 for n in ('RiscRuntimeV1.h','RiscKeyValueAtomicV1.h','RiscKeyValueAvailabilityV1.h','RiscRealtimeV1.h','RiscKeyValueV1.h'):(include/n).write_bytes(subprocess.check_output(['git','-C',str(a.runtime),'show',PIN+':sdk/app/'+n]))
 src=(ROOT/'test/native_apps/portable_update_app_test.c').read_text()
 for path in ('lib/PortableApps/src/adapter.c','Apps/update_portable.inc'):src=src.replace('"../../'+path+'"',json.dumps(str(ROOT/path)))
 src=src.replace('int main(int argc,char**argv)', 'static int original_main(int argc,char**argv)')
 src=src.replace('.acquire=fake_acquire,.release=fake_release', '.acquire=ov_acquire,.release=ov_release,.retain_invocation=ov_retain')
 src=src.replace('static uint16_t pixels[UW*UH];','static uint16_t *pixels;').replace('.stride_bytes=format==RISC_DISPLAY_FORMAT_MONO1?(UW+7)/8:UW*2,.size_bytes=sizeof(pixels)', '.stride_bytes=ov_stride,.size_bytes=ov_bytes')
 fixture=out/'fixture.c';fixture.write_text(src)
 flags=['PORTABLE_UPDATE_NATIVE_UI','PORTABLE_UPDATE_VERSION="1.4.0"','PORTABLE_NATIVE_CANVAS','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NOVA_UI','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_NATIVE','PORTABLE_QUICK_RADIOS','PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_HOME_AUTOMATIC_PREFERENCES','PORTABLE_ALARM_NATIVE','PORTABLE_ALARM_TERMINAL_RETENTION','PORTABLE_CHECKED_STORAGE_RUNTIME','PORTABLE_ASYNC_RGB565','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_WATCH_NATIVE_HEIGHT']
 sources=[ROOT/'test/native_apps/native_overlay_app_store_test.c']+[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]
 inputs=[f for d in ('Apps','lib/PortableApps','lib/NativeApps') for f in (ROOT/d).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc')]+[ROOT/'test/native_apps/native_overlay_app_store_test.c',ROOT/'test/native_apps/native_overlay_providers_test.c',ROOT/'test/native_apps/portable_update_app_test.c',Path(__file__)]
 hashes={str(p.relative_to(ROOT)):sha(p) for p in inputs};rows=[]
 for w,h in ([tuple(map(int,a.size.split('x')))] if a.size else [(128,128),(160,128),(128,160),(240,240),(410,502),(320,180),(502,240)]):
  for snapshot in (0,1):
   binary=out/'test';cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',f'-DUW={w}',f'-DUH={h}',*[f'-D{x}' for x in flags],'-DNATIVE_UPDATE_OVERLAY_FIXTURE='+json.dumps(str(fixture)),'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',str(binary)]
   if snapshot:cmd+=['-DPORTABLE_RASTER_SNAPSHOT']
   if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   r=subprocess.run(cmd,capture_output=True,text=True);(out/f'{w}x{h}-{snapshot}-compile.log').write_text(r.stdout+r.stderr)
   if r.returncode:print(r.stdout+r.stderr,flush=True)
   r.check_returncode()
   for case in ([a.case] if a.case is not None else range(5)):
    r=subprocess.run([str(binary),str(case)],capture_output=True,text=True,timeout=40,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
    (out/f'{w}x{h}-{snapshot}-{case}.log').write_text(r.stdout+r.stderr)
    if r.returncode:print(w,h,snapshot,case,r.stdout,r.stderr,flush=True)
    r.check_returncode();rows.append({'width':w,'height':h,'snapshot':snapshot,'case':case,'passed':True})
   print(w,h,snapshot,'PASS',flush=True);binary.unlink()
 if hashes!={str(p.relative_to(ROOT)):sha(p) for p in inputs}:raise RuntimeError('Source changed; rerun settled source')
 (out/'source-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n');(out/'receipt.json').write_text(json.dumps({'status':'PASS','cases':rows,'sanitized':a.sanitize,'runtime_pin':PIN,'fixture_sha256':sha(fixture),'selected_defines':flags,'hardware_tested':False,'scope':'Real App Store/native Quick/alarm and atomic-preference controllers with bounded providers. No bridge/device fit claim.'},indent=2)+'\n')
if __name__=='__main__':main()
