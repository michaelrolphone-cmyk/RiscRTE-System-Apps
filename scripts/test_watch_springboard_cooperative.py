#!/usr/bin/env python3
"""Real Springboard raster and production cooperative display, explicit work clock.

This is a host regression, not an on-device benchmark. The unchanged display
state machine advances rows only during Runtime yield. Pixel work advances the
fixture clock so the scheduler cannot mistake expensive painting for zero time.
"""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
BASE_REF='f1d8d5d53f80611520feb705f2250295ec07c195'
SDK=ROOT/'test/native_apps/watch_cooperative_sdk'
OUT=ROOT/'build/watch-springboard-cooperative';OUT.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256((ROOT/'test/native_apps/watch_cooperative_display_fixture.h').read_bytes()).hexdigest()=='c754a381be5f1673399fa07e14e4c34e2e551315a9208d2501c0130a6b51a6dc'
san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if os.environ.get('SANITIZE')=='1' else []
results=[]
for label in ['original','fixed']:
 source=ROOT
 with tempfile.TemporaryDirectory(prefix='watch-raster-') as name:
  work=Path(name)
  for d in ['Apps','lib']:
   shutil.copytree(source/d,work/d)
  if label=='original':
   for name in ['adapter.c','canvas_async.inc']:
    rel='lib/PortableApps/src/'+name
    (work/rel).write_bytes(subprocess.check_output(['git','show',BASE_REF+':'+rel],cwd=ROOT))
  p=work/'Apps/springboard_nova.inc';s=p.read_text()
  s=s.replace('static void nova_render(nova_state *s) {','extern void watch_probe_render_begin(void),watch_probe_render_end(void);\nextern void watch_probe_after_poll(t5_app_input_t*);\nstatic void nova_render(nova_state *s) {\n    watch_probe_render_begin();')
  s=s.replace('api->present(false);s->visible_x=', 'watch_probe_render_end();api->present(false);s->visible_x=')
  s=s.replace('while(api->poll(&in,20)) {','while(api->poll(&in,20)) {\n        watch_probe_after_poll(&in);')
  p.write_text(s)
  p=work/'lib/PortableApps/src/adapter.c';s=p.read_text()
  # The real full-feature product enables these existing checkpoints through
  # Contexts. Keep the display checkpoint with no unrelated service mock here.
  if label=='original':
   s=s.replace('static bool canvas_progress(void);','static bool canvas_progress(void);\nstatic uint32_t contexts_pixels;\nstatic bool contexts_capture_checkpoint(void){return canvas_progress();}\n#define PORTABLE_CONTEXTS_RASTER 1',1)
   s=s.replace('#ifdef PORTABLE_CONTEXTS_CLIENT\n    if(!(j&15)', '#if defined(PORTABLE_CONTEXTS_CLIENT) || defined(PORTABLE_ASYNC_RGB565)\n    if(!(j&15)')
  s=s.replace('static void fill(int x,', 'extern void watch_probe_work(void);\nstatic void fill(int x,',1)
  s=s.replace('if(!(j&15)&&!contexts_capture_checkpoint())return;', 'if(!(j&15)){watch_probe_work();if(!contexts_capture_checkpoint())return;}')
  p.write_text(s)
  p=work/'lib/PortableApps/src/nova.inc';s=p.read_text()
  s=s.replace('if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;','if(!(++contexts_pixels&511u)){watch_probe_work();if(failed||!contexts_capture_checkpoint())return;}')
  p.write_text(s)
  binary=OUT/label
  subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-Os',*san,'-Wall','-Wextra','-Werror','-DPORTABLE_ASYNC_RGB565','-DPORTABLE_CATALOG_LIMIT=19','-I'+str(work/'lib/PortableApps/include'),'-I'+str(work/'lib/NativeApps/include'),'-I'+str(SDK),str(work/'Apps/springboard.c'),str(work/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/watch_springboard_cooperative_test.c'),'-o',str(binary)],check=True)
  for cpu in [0,128,512]:
   for period in [1,4]:
    result=json.loads(subprocess.check_output([str(binary),str(cpu),str(period)],text=True));result['variant']=label;results.append(result);print(json.dumps(result),flush=True)
(OUT/('results-sanitized.json' if san else 'results.json')).write_text(json.dumps(results,indent=2)+'\n')
for period in [1,4]:
 old=next(r for r in results if r['variant']=='original' and r['work_us_per512']==512 and r['panel_visit_every_yields']==period)
 new=next(r for r in results if r['variant']=='fixed' and r['work_us_per512']==512 and r['panel_visit_every_yields']==period)
 assert old['max_submit_gap_ms']>new['max_submit_gap_ms']*2,(old,new)
 assert new['completed_before_stop']>old['completed_before_stop']*2,(old,new)
 assert new['paint_yields'] and not old['paint_yields']
 assert old['raster_health_reads']>0
 assert new['raster_health_reads']==0 and new['raster_clock_reads']>0
print('PASS: real Springboard cooperative-provider starvation reproduced and bounded raster pump verified')
