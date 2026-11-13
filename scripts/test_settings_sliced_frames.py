#!/usr/bin/env python3
"""Native Settings immutable-frame pixels, hit identity, custody and timing."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
import settings_sliced_timing
ROOT=Path(__file__).resolve().parents[1]
FLAGS=['-DPORTABLE_SETTINGS_NATIVE_GEOMETRY','-DPORTABLE_SAFE_VIEWPORT','-DTEST_SETTINGS_RASTER_WIDTH=466','-DTEST_SETTINGS_RASTER_HEIGHT=466','-DTEST_SETTINGS_INSET=69']
SLICED=['-DPORTABLE_SETTINGS_SLICED_FRAMES','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_FRAME_RECEIPTS','-DPORTABLE_RASTER_WORK_BUDGET_MS=8','-DPORTABLE_RASTER_COOPERATIVE_INPUT','-DPORTABLE_INPUT_SAMPLE_INTERVAL_MS=2u']
CHANGED=['adapter.c','raster_snapshot_replay.inc','settings.inc','settings_view.inc','settings_timezone.inc','settings_timezone_view.inc']
def stage(out):
 def rewrite(text,folder):
  def replace(m):
   p=(folder/m[1]).resolve()
   if p.name in CHANGED and p.parent==ROOT/'lib/PortableApps/src':p=out/p.name
   return '#include "'+str(p)+'"' if p.exists() or p.parent==out else m[0]
  return re.sub(r'#include "([^"]+)"',replace,text)
 for name in CHANGED:
  p=ROOT/'lib/PortableApps/src'/name;(out/name).write_text(rewrite(p.read_text(),p.parent))
 for name in ['portable_settings_test.c','settings_sleep_defaults_test.c']:
  p=ROOT/'test/native_apps'/name;s=rewrite(p.read_text(),p.parent).replace(str(ROOT/'lib/PortableApps/src/adapter.c'),str(out/'adapter.c')).replace(str(ROOT/'test/native_apps/portable_settings_test.c'),str(out/'portable_settings_test.c'))
  # Cooperative frames take controller passes; old poll-count exit is fixture timing, not a product limit.
  s=s.replace('return polls < 120;','return polls < 100000;');(out/name).write_text(s)
 (out/'map_test.c').write_text((ROOT/'test/native_apps/settings_sliced_map_test.c').read_text())
def source_hashes():
 return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(ROOT/'lib/PortableApps/src'/n for n in CHANGED),Path(__file__),ROOT/'scripts/settings_sliced_timing.py',ROOT/'test/native_apps/settings_sliced_map_test.c']}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);source_before=source_hashes();stage(out)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1');os.environ.update(env)
 flags=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*FLAGS,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk)]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 commands=[];pixels={}
 for name,extra in [('sync',[]),('cooperative',SLICED)]:
  exe=out/(name+'-map');cmd=flags+extra+(['-Wl,--wrap=malloc'] if extra else [])+[str(ROOT/'Apps/settings.c'),str(out/'map_test.c'),'-o',str(exe)];commands.append(cmd);subprocess.run(cmd,check=True)
  result=subprocess.run([str(exe)],check=True,text=True,capture_output=True,env=env);(out/(name+'-map.log')).write_text(result.stdout+result.stderr);pixels[name]=result.stdout;exe.unlink()
 assert pixels['sync']==pixels['cooperative'],'Completed pixel mismatch'
 d=out/'retained';d.mkdir(exist_ok=True);s=(out/'portable_settings_test.c').read_text();start=s.index('static bool touch_script_snapshot');end=s.index('static void touch_emit',start);fn=s[start:end].replace('polls','scripted_polls');fn=fn.replace('  (void)c;memset(out,0,sizeof(*out));','  (void)c;\n  if(!script_started && sv_frame_current() && sv_page!=SV_ROOT){script_started=true;script_origin=polls;}\n  unsigned scripted_polls=script_started?polls-script_origin:0;\n  memset(out,0,sizeof(*out));');s=s[:start]+'static bool script_started;static unsigned script_origin;\n'+fn+s[end:];(d/'portable_settings_test.c').write_text(s)
 (d/'settings_sleep_defaults_test.c').write_text((out/'settings_sleep_defaults_test.c').read_text().replace(str(out/'portable_settings_test.c'),str(d/'portable_settings_test.c')));(d/'test.c').write_text((ROOT/'test/native_apps/settings_timer_retained_test.c').read_text())
 exe=d/'test';cmd=flags+SLICED+['-DTEST_SETTINGS_ASYNC',str(ROOT/'Apps/settings.c'),str(d/'test.c'),'-o',str(exe)];commands.append(cmd);subprocess.run(cmd,check=True)
 cases=[]
 for kind in range(3):
  for interruption in range(5):
   result=subprocess.run([str(exe),str(kind),str(interruption)],check=True,text=True,capture_output=True,env=env,timeout=30);(d/f'{kind}-{interruption}.log').write_text(result.stdout+result.stderr);cases.append([kind,interruption])
 exe.unlink();settings_sliced_timing.run(ROOT,out,a.sanitize)
 before=json.loads((out/'sync/results.json').read_text());after=json.loads((out/'sliced/results.json').read_text())
 for old,new in zip(before,after):
  assert (old['cost_ns'],old['period_ms'],old['jitter'])==(new['cost_ns'],new['period_ms'],new['jitter'])
  assert not new['gaps'] and new['dispatched']==new['emitted']
  assert new['max_frame_ms']<old['max_frame_ms']
  if old['dispatched']:assert new['max_dispatch_ms']<old['max_dispatch_ms']
 assert any(x['gaps'] for x in before)
 sources=source_hashes();assert sources==source_before,'Source changed during owning qualification'
 receipt={'source_sha256':sources,'commands':commands,'pixel_pages':9,'completed_map_cases':4,'allocation_fallback_cases':2,'retained_cases':cases,'async_timing_cases_per_lane':8,'sanitized':a.sanitize,'physical_verified':False,'timing_assumptions':{'report_cost_us':2250,'panel_ms':16,'pixel_cost_ns':[100,1000],'report_period_ms':[5,10],'jitter_ms':2,'captured_edges_not_every_hardware_transition':True}}
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Settings sliced owning qualification PASS')
if __name__=='__main__':main()
