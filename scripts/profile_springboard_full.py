#!/usr/bin/env python3
"""Full-feature native controller and production raster profile, never hardware FPS."""
from pathlib import Path
import argparse,json,os,shutil,subprocess,tempfile,statistics
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,default=ROOT)
p.add_argument('--bridges',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--sanitize',action='store_true')
p.add_argument('--instrument',action='store_true')
p.add_argument('--stage-profile',action='store_true',help='Compile the deployed PORTABLE_STAGE_PROFILE diagnostics')
p.add_argument('--verify',action='store_true')
p.add_argument('--repeats',type=int,default=1,help='repeat each native host measurement, preserving all features')
a=p.parse_args();assert 1<=a.repeats<=32
a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
assert subprocess.check_output(['git','-C',str(a.bridges),'rev-parse','HEAD'],text=True).strip()=='6e82fdcaec0b73ed700d2104cfb4d94f011c19d4'
with tempfile.TemporaryDirectory(prefix='full-render-profile-') as temporary:
 work=Path(temporary)
 for d in ('Apps','lib','test'):shutil.copytree(a.source/d,work/d)
 # Existing healthy ABI fixtures retain every provider and feature in the actual recipe.
 probe=(ROOT/'docs/t063/healthy-controller/probe24.c').read_text()
 if 'no 240-wide proxy' in (a.source/'Apps/springboard.c').read_text():
  probe=probe.replace('assert(api->screen_width()==240 && api->screen_height()==logical_height());','assert(api->screen_width()==(int)fw && api->screen_height()==(int)fh);')
 hooks=[('static bool ctx_capture(void*c){','profile_calls[0]++;'),('static bool nt_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {','profile_calls[1]++;'),('static bool nt_monotonic(uint32_t *out) {','profile_calls[2]++;'),('static void nt_yield(uint32_t n) {','profile_calls[3]++;'),('static bool nt_poll_touch(void *c,size_t n) {','profile_calls[4]++;')]
 for marker,insert in hooks:
  assert probe.count(marker)==1;probe=probe.replace(marker,marker+insert)
 fixture=work/'profile-fixture.c';fixture.write_text(probe)
 if a.instrument:
  hooks=[('lib/PortableApps/src/nova.inc','static void np_pixel(int x,int y,uint32_t rgb,unsigned a) {','profile_calls[5]++;'),('lib/PortableApps/src/watch_native_layout.inc','static void native_watch_pixel(int x,int y,uint32_t rgb,unsigned alpha) {','profile_calls[6]++;'),('lib/PortableApps/src/nova.inc','static unsigned np_root(unsigned n) {','profile_calls[7]++;')]
  for file,marker,insert in hooks:
   path=work/file;s=path.read_text();assert s.count(marker)==1;s=s.replace(marker,marker+insert);path.write_text(s)
 header=work/'instrument.h';header.write_text('#include <stdint.h>\nextern uint64_t profile_calls[16];\n')
 paths=dict(fixture=str(ROOT/'docs/t063/healthy-controller'),system=str(work),bridge=str(a.bridges.resolve()),output=str(a.output))
 command=[x.format_map(paths) for x in json.loads((ROOT/'docs/t063/healthy-controller/command24.json').read_text())]
 command=[str(ROOT/'test/native_apps/springboard_full_profile_test.c') if x==str(ROOT/'docs/t063/healthy-controller/probe24.c') else x for x in command]
 command[command.index('-O1')]='-Os'
 command+=['-D_POSIX_C_SOURCE=200809L','-flto','-I'+str(ROOT),'-DPROFILE_FIXTURE="'+str(fixture)+'"','-include',str(header)]
 if a.stage_profile:command+=['-DPORTABLE_STAGE_PROFILE']
 if a.sanitize:command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 (a.output/'command.json').write_text(json.dumps(command,indent=2)+'\n');subprocess.run(command,check=True)
 results=[]
 for repetition,w,h in [(r,w,h) for r in range(a.repeats) for w,h in ([(160,160),(239,293),(240,240),(320,320),(320,390),(410,502),(800,1000)] if a.verify else [(240,240),(410,502)])]:
  captures=a.output/f'{w}x{h}';captures.mkdir(exist_ok=True)
  run=subprocess.run([str(a.output/'probe24'),str(w),str(h),str(captures)],env={**os.environ,**({'VERIFY_NATIVE':'1'} if a.verify else {}),'ASAN_OPTIONS':'detect_leaks=0'},check=True,text=True,capture_output=True,timeout=120)
  (a.output/f'{w}x{h}-{repetition}.log').write_text(run.stdout+run.stderr)
  row=({'width':w,'height':h,'result':next(x for x in run.stdout.splitlines() if x.startswith('NATIVE_PASS '))} if a.verify else json.loads(next(x.removeprefix('PROFILE ') for x in run.stdout.splitlines() if x.startswith('PROFILE '))));row['repetition']=repetition;results.append(row);print(json.dumps(row),flush=True)
 (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 if not a.verify:
  summary=[]
  for w,h in [(240,240),(410,502)]:
   group=[r for r in results if r['width']==w and r['height']==h]
   assert len({r['pixels_hash'] for r in group})==1 and len({r['state_hash'] for r in group})==1
   summary.append({'width':w,'height':h,'repetitions':a.repeats,'frames_per_repetition':240,
    'timing_basis':'host CLOCK_THREAD_CPUTIME_ID; full-feature provider model; not hardware latency or FPS',
    'measurements':{key:{'median':statistics.median(r[key] for r in group),'min':min(r[key] for r in group),'max':max(r[key] for r in group)} for key in group[0] if key.endswith('_us_per_frame')},
    'pixels_hash':group[0]['pixels_hash'],'state_hash':group[0]['state_hash'],'instrument_calls':group[0]['instrument_calls']})
  (a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
