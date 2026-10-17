#!/usr/bin/env python3
"""RGB behavior controls for the paper coordinate guard, with optional baseline pairing."""
import argparse,hashlib,json,os,re,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--baseline-root',type=Path);p.add_argument('--normal-only',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);rows=[];oracles={}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def replace_once(text,before,after):
 assert text.count(before)==1,before
 return text.replace(before,after)
for root_name,root in [('candidate',ROOT)]+([('baseline',a.baseline_root.resolve())] if a.baseline_root else []):
 fixture=(root/'test/native_apps/portable_handoff_test.c').read_text()
 for before,after in {
  '#include "../../lib/PortableApps/src/adapter.c"':'#include '+json.dumps(str(root/'lib/PortableApps/src/adapter.c')),
  'mock_pixels[240*243]':'*mock_pixels','sizeof(mock_pixels)':'geometry_size',
  '.width=240,.height=240':'.width=geometry_width,.height=geometry_height',
  '.stride_bytes=486,':'.stride_bytes=geometry_stride,','mock_owned[4]':'mock_owned[128]','i<4':'i<128',
  '.acquire=mock_acquire,.release=mock_release':'.acquire=ana_acquire,.release=mock_release',
  '.present_status=mock_status};':'.present_status=mock_status,.set_brightness=ana_brightness};',
  'mock_poll_touch,mock_next,mock_snapshot':'ana_poll,ana_next,mock_snapshot',
 }.items():
  assert before in fixture,before
  fixture=fixture.replace(before,after)
 frame=out/(root_name+'-frame.c');frame.write_text(fixture)
 test=(root/'test/native_apps/alarm_native_adapter_test.c').read_text()
 test=replace_once(test,'static void raw_position(int x,int y) {','''static void raw_position(int x,int y) {
#if defined(PORTABLE_WATCH_NATIVE_HEIGHT) && !defined(PORTABLE_ALARM_NATIVE)
 /* Legacy fixture gestures are in its 240-unit view; provider events are native.
  * Ceil chooses the first native pixel whose adapter projection is that unit. */
 x=(x*(int)geometry_width+239)/240;y=(y*(int)geometry_width+239)/240;
#endif''')
 test=replace_once(test,'assert(w==240&&h==240);layout=', '''assert((w==240&&h==240)
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
 ||(w==410&&h==502)
#endif
 );layout=''')
 test=replace_once(test,'const unsigned sizes[][2]={{240,240}};', '''const unsigned sizes[][2]={{240,240}
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
 ,{410,502}
#endif
 };''')
 source=out/(root_name+'-test.c');source.write_text(test)
 for sanitized in ([False] if a.normal_only else [False,True]):
  for native in (False,True):
   for height in (False,True):
    name=f'{root_name}-native{int(native)}-height{int(height)}-san{int(sanitized)}';exe=out/'test-binary'
    flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RETAINED_RGB565_HANDOFF','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_TOUCH_ROTATION=0']
    if native:flags+=['-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_ALARM_NATIVE']
    if height:flags+=['-DPORTABLE_WATCH_NATIVE_HEIGHT']
    if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-DFRAME_FIXTURE='+json.dumps(str(frame)),'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(source)]
    dep=subprocess.run([cmd[0],'-MM',*cmd[1:]],text=True,capture_output=True,check=True);(out/(name+'.d')).write_text(dep.stdout);dependencies={}
    for token in shlex.split(dep.stdout.replace('\\\n',' ')):
     if token.endswith(':'):continue
     path=Path(os.path.normpath(token));key='generated:'+path.name if path.parent==out else str(path.relative_to(root));dependencies[key]=sha(path)
    row={'name':name,'source':root_name,'native':native,'height':height,'sanitized':sanitized,'dependencies':dependencies,'command':cmd}
    try:
     build=subprocess.run([*cmd,'-o',str(exe)],text=True,capture_output=True,timeout=180);(out/(name+'-build.log')).write_text(build.stdout+build.stderr);build.check_returncode();row['binary_sha256']=sha(exe)
     run=subprocess.run([str(exe)],text=True,capture_output=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});(out/(name+'.log')).write_text(run.stdout+run.stderr);row.update(returncode=run.returncode,output=run.stdout+run.stderr)
     rows.append(row);(out/'receipt.json').write_text(json.dumps({'hardware':False,'rows':rows,'runner_sha256':sha(Path(__file__))},indent=2)+'\n');print(name,run.returncode,flush=True)
     if run.returncode:print(run.stdout+run.stderr,flush=True)
     run.check_returncode();render=dict(re.findall(r'^RENDER (.+) ([0-9a-f]{16})$',run.stdout,re.M));assert render
     for key,value in render.items():
      identity=(native,key);assert identity not in oracles or oracles[identity]==value,(name,key);oracles[identity]=value
    finally:
     if exe.exists():exe.unlink()
print(f'PASS {len(rows)} RGB behavior controls; exact render hashes across equivalent source/selector/sanitizer rows')
