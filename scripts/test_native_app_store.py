#!/usr/bin/env python3
"""Run production App Store/controller/native raster against bounded providers."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PIN='48229bd1593a2e24e95bf4537b495ee615990fd3'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--snapshot',action='store_true');p.add_argument('--size');p.add_argument('--case',type=int);p.add_argument('--captures',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
 (include/'RiscRuntimeV1.h').write_bytes(subprocess.check_output(['git','-C',str(a.runtime),'show',PIN+':sdk/app/RiscRuntimeV1.h']))
 src=(ROOT/'test/native_apps/portable_update_app_test.c').read_text()
 for path in ('lib/PortableApps/src/adapter.c','Apps/update_portable.inc'):src=src.replace('"../../'+path+'"',json.dumps(str(ROOT/path)))
 src=src.replace('int main(int argc,char**argv)', 'static int original_main(int argc,char**argv)')
 src=src.replace('.request_launch=fake_launch,', '.request_launch=fake_launch,.retain_invocation=native_retain,.exit_requested=native_exit,')
 src=src.replace('script[i].at==input_step() && script[i].x>=0','input_step()>=script[i].at&&input_step()<script[i].at+4&&script[i].x>=0')
 src=src.replace('out->version,"1.1.0"','out->version,native_release_version')
 src=src.replace('.flags=UI_FLAGS}', '.flags=UI_FLAGS|RISC_DISPLAY_INFO_ASYNC_PRESENT}')
 src=src.replace('*t=++presents;', '*t=++presents;native_present_deadline=ticks+native_present_delay;')
 src=src.replace('out->state=RISC_DISPLAY_PRESENT_COMPLETE;', 'out->state=ticks<native_present_deadline?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;')
 src=src.replace('ticks+=ms;', 'ticks+=ms;native_tick();')
 # Contact coordinates remain native; only the legacy fixture helper transforms.
 src=src.replace('#ifdef PORTABLE_WATCH_NATIVE_HEIGHT\n if(x>=0)', '#if defined(PORTABLE_WATCH_NATIVE_HEIGHT) && !defined(PORTABLE_UPDATE_NATIVE_UI)\n if(x>=0)')
 src=src.replace('239-out->contacts[0].x','UW-1-out->contacts[0].x').replace('239-out->contacts[0].y','UH-1-out->contacts[0].y')
 fixture=out/'fixture.c';fixture.write_text(src)
 paths=[f for d in ('Apps','lib/PortableApps','lib/NativeApps') for f in (ROOT/d).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc')]+[ROOT/'test/native_apps/native_app_store_test.c',ROOT/'test/native_apps/portable_update_app_test.c',Path(__file__)]
 hashes={str(f.relative_to(ROOT)):sha(f) for f in paths};rows=[]
 sizes=[tuple(map(int,a.size.split('x')))] if a.size else [(128,128),(160,128),(128,160),(240,240),(410,502),(320,180),(502,240)]
 for w,h in sizes:
  for rot in (0,180):
   binary=out/'test';cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',f'-DUW={w}',f'-DUH={h}',f'-DPORTABLE_TOUCH_ROTATION={rot}','-DNATIVE_APP_STORE_FIXTURE='+json.dumps(str(fixture)),'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/native_app_store_test.c'),'-o',str(binary)]
   if a.snapshot:cmd+=['-DPORTABLE_RASTER_SNAPSHOT']
   if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   subprocess.run(cmd,check=True)
   for case in ([a.case] if a.case is not None else range(24)):
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    if a.captures and case==0:
     capture=out/f'{w}x{h}-rotation{rot}';capture.mkdir(exist_ok=True);env['PORTABLE_UPDATE_FRAME_DIR']=str(capture)
    result=subprocess.run([str(binary),str(case)],env=env,capture_output=True,text=True,timeout=30)
    (out/f'{w}x{h}-{rot}-{case}.log').write_text(result.stdout+result.stderr)
    if result.returncode:print(w,h,rot,case,result.stdout,result.stderr,flush=True)
    result.check_returncode();rows.append({'width':w,'height':h,'rotation':rot,'case':case,'passed':True})
   print(f'{w}x{h} rotation{rot}: passed',flush=True);binary.unlink()
 if hashes!={str(f.relative_to(ROOT)):sha(f) for f in paths}:raise RuntimeError('Source changed; rerun settled source')
 (out/'source-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
 (out/'receipt.json').write_text(json.dumps({'status':'PASS','cases':rows,'sanitized':a.sanitize,'snapshot':a.snapshot,'runtime_pin':PIN,'runtime_header_sha256':sha(include/'RiscRuntimeV1.h'),'fixture_sha256':sha(fixture),'hardware_tested':False,'scope':'Production App Store controller, native list renderer, glyph raster, raw input and checked custody with bounded synthetic providers.'},indent=2)+'\n')
if __name__=='__main__':main()
