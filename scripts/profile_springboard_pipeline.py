#!/usr/bin/env python3
"""Real Springboard raster and production cooperative display, explicit work clock.

This is a host regression, not an on-device benchmark. The unchanged display
state machine advances rows only during Runtime yield. Pixel work advances the
fixture clock so the scheduler cannot mistake expensive painting for zero time.
"""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
BASE_REF='f74db7dda4d4a6d5fffa048a1e9384addab1540c'
SDK=ROOT/'test/native_apps/watch_cooperative_sdk'
OUT=ROOT/'build/t030-pipeline';OUT.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256((ROOT/'test/native_apps/watch_cooperative_display_fixture.h').read_bytes()).hexdigest()=='c754a381be5f1673399fa07e14e4c34e2e551315a9208d2501c0130a6b51a6dc'
san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if os.environ.get('SANITIZE')=='1' else []
results=[]
for label in ['watch-reference','original','fixed']:
 source=ROOT
 with tempfile.TemporaryDirectory(prefix='watch-raster-') as name:
  work=Path(name)
  for d in ['Apps','lib']:
   shutil.copytree(source/d,work/d)
  if label!='fixed':
   for name in ['nova.inc','adapter.c']:
    rel='lib/PortableApps/src/'+name
    (work/rel).write_bytes(subprocess.check_output(['git','show',BASE_REF+':'+rel],cwd=ROOT))
  p=work/'Apps/springboard_nova.inc';s=p.read_text()
  s=s.replace('static void nova_render(nova_state *s) {','extern void watch_probe_render_begin(void),watch_probe_render_end(void);\nextern void watch_probe_after_poll(t5_app_input_t*);\nstatic void nova_render(nova_state *s) {\n    watch_probe_render_begin();')
  s=s.replace('api->present(false);s->visible_x=', 'watch_probe_render_end();api->present(false);s->visible_x=')
  s=s.replace('while(api->poll(&in,20)) {','while(api->poll(&in,20)) {\n        watch_probe_after_poll(&in);')
  p.write_text(s)
  p=work/'lib/PortableApps/src/adapter.c';s=p.read_text()
  s=s.replace('static void fill(int x,', 'extern void watch_probe_work(void);\nstatic void fill(int x,',1)
  s=s.replace('if(!(j&15)&&!contexts_capture_checkpoint())return;', 'if(!(j&15)){watch_probe_work();if(!contexts_capture_checkpoint())return;}')
  s=s.replace('    uint32_t slice=remaining;', '    uint32_t slice=remaining>8?8:remaining;')
  # The deployed recipe enables Contexts. Model its exact 8ms wait slices
  # without unrelated service mocks; canvas checkpoints above remain real.
  if label!='fixed':
   s=s.replace('  rt->yield_ms(spent<wait?wait-spent:1);', '  uint32_t left=spent<wait?wait-spent:1;while(left){uint32_t slice=left>8?8:left;rt->yield_ms(slice);left-=slice;if(!canvas_progress() || !contexts_capture_checkpoint())return false;}')
  p.write_text(s)
  p=work/'lib/PortableApps/src/nova.inc';s=p.read_text()
  s=s.replace('if(failed||(!(++contexts_pixels&511u)&&!contexts_capture_checkpoint()))return;','if(!(++contexts_pixels&511u)){watch_probe_work();if(failed||!contexts_capture_checkpoint())return;}')
  p.write_text(s)
  fixture=work/'probe.c'
  fixture_text=(ROOT/'test/native_apps/watch_springboard_cooperative_test.c').read_text()
  fixture_text=fixture_text.replace('#define WIDTH 240u','#define WIDTH 410u').replace('#define HEIGHT 240u','#define HEIGHT 502u').replace('#define STRIDE 480u','#define STRIDE 820u')
  fixture_text=fixture_text.replace('out->nominal_refresh_millihz=20000','out->nominal_refresh_millihz=0')
  fixture_text=fixture_text.replace('.width=240,.height=240','.width=WIDTH,.height=HEIGHT').replace('.x=x,.y=120','.x=(x*WIDTH+239)/240,.y=HEIGHT/2')
  if label=='watch-reference':
   fixture_text=fixture_text.replace('#define WIDTH 410u','#define WIDTH 240u').replace('#define HEIGHT 502u','#define HEIGHT 240u').replace('#define STRIDE 820u','#define STRIDE 480u')
  fixture.write_text(fixture_text)
  binary=OUT/label
  subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-Os',*san,'-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_ASYNC_RGB565','-DPORTABLE_WATCH_NATIVE_HEIGHT','-I'+str(ROOT/'test/native_apps'),'-DPORTABLE_CATALOG_LIMIT=19','-I'+str(work/'lib/PortableApps/include'),'-I'+str(work/'lib/NativeApps/include'),'-I'+str(SDK),str(work/'Apps/springboard.c'),str(work/'lib/PortableApps/src/adapter.c'),str(fixture),'-o',str(binary)],check=True)
  for cpu in [0,128,512]:
   for period in [1,4]:
    result=json.loads(subprocess.check_output([str(binary),str(cpu),str(period)],text=True));result['variant']=label;results.append(result);print(json.dumps(result),flush=True)
(OUT/('results-sanitized.json' if san else 'results.json')).write_text(json.dumps(results,indent=2)+'\n')
# CPU work and panel row latency are explicit models, never target timings.
for r in results:
 assert r['completed_before_stop']>0
for period in (1,4):
 old=next(r for r in results if r['variant']=='original' and r['work_us_per512']==0 and r['panel_visit_every_yields']==period)
 new=next(r for r in results if r['variant']=='fixed' and r['work_us_per512']==0 and r['panel_visit_every_yields']==period)
 assert new['completed_before_stop']>old['completed_before_stop'],(old,new)
 assert new['max_submit_gap_ms']<old['max_submit_gap_ms'],(old,new)
print('PASS: production cooperative display drains immutable frames at native geometry')
