#!/usr/bin/env python3
"""Actual CST provider/reducer/Nova diagnostic and pixel parity; modeled media."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--drivers',type=Path,required=True);p.add_argument('--provider-sdk',type=Path,required=True);p.add_argument('--catalog',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 rows=[];parity={};dependencies=set([Path(__file__).resolve()]);commands=[]
 with tempfile.TemporaryDirectory(prefix='gesture-',dir=a.output) as tmp:
  out=Path(tmp);inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
  for h in (a.runtime/'sdk/app').glob('*.h'):shutil.copyfile(h,inc/h.name);dependencies.add(h)
  catalog=json.loads(a.catalog.read_text())['apps'];dependencies.add(a.catalog)
  (out/'watch_suite_catalog.inc').write_text('\n'.join('{.display_name='+json.dumps(x['display_name'])+',.file_name='+json.dumps(x['file_name'])+',.icon='+json.dumps(x['icon'])+',.compatible=true},' for x in catalog)+'\n')
  nova=ROOT/'Apps/springboard_nova.inc';s=nova.read_text();s=s.replace('static void nova_cancel(nova_state *s) {','extern void direction_cancel(void);\nstatic void nova_cancel(nova_state *s) {direction_cancel();',1);needle='nova_tick(&s,elapsed);s.last_motion=now;';assert s.count(needle)==1;s=s.replace(needle,needle+'\nextern void direction_state(float,float,int,int,bool,bool,bool);direction_state(s.motion.x,s.motion.y,s.visible_x,s.visible_y,s.down,s.visible_valid,s.motion.settled);');(out/'observed-nova.inc').write_text(s)
  app=ROOT/'Apps/springboard.c';(out/'app.c').write_text(app.read_text().replace('#include "springboard_nova.inc"','#include '+json.dumps(str(out/'observed-nova.inc'))))
  adapter=ROOT/'lib/PortableApps/src/adapter.c';s=adapter.read_text();s=s.replace('static bool raster_checkpoint(void) {','extern void audit_work(unsigned);\nstatic bool raster_checkpoint(void) {\naudit_work(raster_pixels);',1).replace('static void clear_color(uint16_t color) {','extern void audit_frame_begin(void);\nstatic void clear_color(uint16_t color) {\naudit_frame_begin();',1);(out/'adapter.c').write_text(s)
  dependencies.update([nova,app,adapter]);flags=['-std=c11','-O1','-Wall','-Wextra','-Werror'];san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
  objects=[]
  for src in [ROOT/'test/native_apps/springboard_gesture_cst_bridge.c',a.drivers/'Drivers/cst9217_touch/driver.c']:
   obj=out/(src.stem+'.o');cmd=['cc',*flags,*san,'-I'+str(a.provider_sdk),'-I'+str(a.drivers/'test/drivers'),'-I'+str(a.drivers/'components/co5300/include'),'-c',str(src),'-o',str(obj)];subprocess.run(cmd,check=True);commands.append(cmd);objects.append(str(obj));dependencies.add(src)
  for rotation in (0,180):
   for enabled in (False,True):
    defs=['PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NOVA_UI','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_FORCE_FULL_FRAMES','TEST_WIDTH=466','TEST_HEIGHT=466','NOVA_WATCH_CATALOG','CATALOG_COUNT='+str(len(catalog)),'PORTABLE_RASTER_WORK_BUDGET_MS=8','PORTABLE_RASTER_SNAPSHOT','PORTABLE_NOVA_SLICED_FRAMES','PORTABLE_FRAME_RECEIPTS','PORTABLE_INPUT_SAMPLE_INTERVAL_MS=2u','PORTABLE_RASTER_COOPERATIVE_INPUT','PORTABLE_RGB565_SPAN_FILL','PORTABLE_PERFORMANCE_TRACE','PORTABLE_TOUCH_ROTATION='+str(rotation),'TEST_DIAG_OFF='+str(int(not enabled))]+(['PORTABLE_SPRINGBOARD_GESTURE_TRACE'] if enabled else [])
    includes=[inc,out,ROOT/'Apps',ROOT/'lib/PortableApps/src',ROOT/'lib/NativeApps/include'];srcs=[out/'app.c',out/'adapter.c',ROOT/'test/native_apps/springboard_gesture_adapter_test.c'];exe=out/'test';cmd=['cc',*flags,*san,*['-D'+x for x in defs],*['-I'+str(x) for x in includes],*map(str,srcs),*objects,'-Wl,--wrap=t5_app_get_api','-o',str(exe)];commands.append(cmd);subprocess.run(cmd,check=True)
    for gap in (False,True):
     for direction in range(4):
      env={**os.environ,'DIRECTION':str(direction),'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'};env.pop('TEST_DIAG_GAP',None)
      if gap:env['TEST_DIAG_GAP']='1'
      v=subprocess.run([str(exe),'100','2','250','40','4'],capture_output=True,text=True,env=env,timeout=60);name=f'{rotation}-{int(enabled)}-{int(gap)}-{direction}';(a.output/(name+'.log')).write_text(v.stdout+v.stderr);v.check_returncode();signature=next(x for x in v.stdout.splitlines() if x.startswith('PARITY '));key=(rotation,gap,direction)
      if key in parity:assert parity[key]==signature,(key,signature,parity[key])
      else:parity[key]=signature
      rows.append(dict(rotation=rotation,enabled=enabled,gap=gap,direction=direction,signature=signature,passed=True));print('PASS',name,flush=True)
    exe.unlink()
  dependencies.update(h for h in a.provider_sdk.rglob('*') if h.is_file());dependencies.add(a.drivers/'test/drivers/cst9217_test_model.h')
  dependencies.update(ROOT/'test/native_apps'/x for x in ['springboard_gesture_adapter_test.c','springboard_gesture_cst_bridge.c']);dependencies.update(x for x in (ROOT/'lib/PortableApps').rglob('*') if x.is_file())
  (a.output/'receipt.json').write_text(json.dumps(dict(passed=True,sanitized=a.sanitize,runs=rows,commands=commands,source_sha256={str(x):sha(x) for x in sorted(dependencies)},modeled=True,physical=False),indent=2)+'\n')
if __name__=='__main__':main()
