#!/usr/bin/env python3
"""Replacement native pipeline: host CPU timing and calibrated provider fake-clock timing."""
from pathlib import Path
import argparse,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--bridges',type=Path,required=True)
p.add_argument('--provider-model',type=Path,default=ROOT/'test/native_apps/fixtures/springboard_provider_model')
p.add_argument('--wave',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--sanitize',action='store_true')
p.add_argument('--stage-profile',action='store_true',help='Compile the deployed PORTABLE_STAGE_PROFILE diagnostics')
p.add_argument('--old-wait',action='store_true',help='Compare only prior wait accounting on the new native raster')
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='native-cadence-') as temp:
 work=Path(temp)
 for d in ('Apps','lib','test'):shutil.copytree(ROOT/d,work/d)
 probe=(ROOT/'docs/t063/healthy-controller/probe24.c').read_text()
 def replace_function(s,marker,body):
  begin=s.index(marker)+len(marker);end=begin;level=1
  while level:
   if s[end]=='{':level+=1
   elif s[end]=='}':level-=1
   end+=1
  return s[:begin]+body+s[end-1:]
 probe=replace_function(probe,'static bool nt_frame(void *c,uint32_t format,risc_display_surface_v1 *out) {','(void)c;assert(!frames);bool ok=model_display_api()->acquire(NULL,format,out);if(ok)frames=1;return ok;')
 probe=replace_function(probe,'static void nt_frame_release(void *c,risc_display_frame_v1 f) {','(void)c;assert(frames);model_display_api()->release(NULL,f);frames=0;')
 probe=replace_function(probe,' const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token) {','(void)c;assert(frames);bool ok=model_display_api()->submit(NULL,f,r,n,options,token);if(ok){frames=0;presents++;}return ok;')
 probe=replace_function(probe,'static bool nt_status(void *c,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {','(void)c;cadence_calls[1]++;return model_display_api()->present_status(NULL,token,out);')
 probe=replace_function(probe,'static bool nt_monotonic(uint32_t *out) {','cadence_calls[2]++;ticks=model_now_ms();*out=ticks;return true;')
 probe=replace_function(probe,'static bool nt_health(risc_runtime_health_v1 *out) {','cadence_calls[8]++;ticks=model_now_ms();out->uptime_ms=ticks;return true;')
 probe=replace_function(probe,'static void nt_yield(uint32_t n) {','cadence_calls[3]++;model_yield(n?n:1);ticks=model_now_ms();')
 probe=replace_function(probe,'static bool ctx_capture(void*c){','(void)c;cadence_calls[0]++;model_advance_us(cadence_capture_us);return true;')
 probe=probe.replace('static bool nt_poll_touch(void *c,size_t n) {','static bool nt_poll_touch(void *c,size_t n) {cadence_calls[4]++;ticks=model_now_ms();')
 fixture=work/'cadence-fixture.c';fixture.write_text(probe)
 header=work/'cadence-hooks.h';header.write_text('#include <stdint.h>\n#include "provider_model.h"\nextern uint64_t cadence_calls[32];\nextern unsigned cadence_capture_us,cadence_raster_us;\n')
 path=work/'lib/PortableApps/src/adapter.c';s=path.read_text();marker='static bool contexts_capture_checkpoint(void) {'
 assert s.count(marker)==2;s=s.replace(marker,marker+'cadence_calls[5]++;if(surface.frame)model_advance_us(cadence_raster_us);',1)
 if a.old_wait:
  s=s.replace('  const uint32_t budget=remaining,started=now;\n','')
  begin=s.index('    /* Runtime performs bounded provider work before its delay. Count that');end=s.index('\n  }\n#else',begin);s=s[:begin]+s[end:]
 path.write_text(s)
 path=work/'test/native_apps/fixtures/installed_catalog_provider.h';s=path.read_text()
 s=s.replace('static int32_t fixture_catalog_snapshot(void *context,risc_app_catalog_snapshot_v1 *out) {','static int32_t fixture_catalog_snapshot(void *context,risc_app_catalog_snapshot_v1 *out) {cadence_calls[6]++;')
 s=s.replace('static int32_t fixture_catalog_get(void *context,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1 *out) {','static int32_t fixture_catalog_get(void *context,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1 *out) {cadence_calls[7]++;');path.write_text(s)
 paths=dict(fixture=str(ROOT/'docs/t063/healthy-controller'),system=str(work),bridge=str(a.bridges.resolve()),output=str(a.output))
 command=[x.format_map(paths) for x in json.loads((ROOT/'docs/t063/healthy-controller/command24.json').read_text())]
 command=[str(work/'test/native_apps/springboard_cadence_test.c') if x==str(ROOT/'docs/t063/healthy-controller/probe24.c') else x for x in command]
 command[command.index('-O1')]='-Os'
 command+=['-D_POSIX_C_SOURCE=200809L','-flto','-DPROFILE_FIXTURE="'+str(fixture)+'"','-include',str(header),'-I'+str(a.provider_model.resolve()),'-I'+str(a.wave.resolve()/'tests'),'-I'+str(a.wave.resolve()/'include'),str(a.output/'provider-model.o')]
 if a.stage_profile:command+=['-DPORTABLE_STAGE_PROFILE']
 if a.sanitize:command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 provider_command=['cc','-std=c11','-Os','-flto','-Drisc_runtime_get_api=model_risc_runtime_get_api','-DPROFILE_PANEL_PROVIDER_SOURCE=\"'+str(a.wave.resolve()/'drivers/panel_vendor/driver.c')+'\"','-Wall','-Wextra','-Werror',*[('-I'+str(i)) for i in (a.wave.resolve()/'tests',a.wave.resolve()/'include',a.bridges.resolve()/'sdk/driver',a.bridges.resolve()/'sdk/app',a.bridges.resolve()/'include')],'-c',str(a.provider_model.resolve()/'provider_model.c'),'-o',str(a.output/'provider-model.o')]
 if a.sanitize:provider_command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 (a.output/'provider-command.json').write_text(json.dumps(provider_command,indent=2)+'\n');subprocess.run(provider_command,check=True)
 (a.output/'command.json').write_text(json.dumps(command,indent=2)+'\n');subprocess.run(command,check=True)
 results=[]
 for pair,capture,raster in [(84,0,0),(250,0,0),(1000,0,0),(84,1,0),(84,0,25),(84,0,100)]:
  # LeakSanitizer cannot enumerate threads in a ptrace-supervised executor.
  # This fixture asserts all allocations/grants/frames are released itself;
  # ASan bounds/lifetime checks and UBSan remain enabled.
  run=subprocess.run([str(a.output/'probe24'),str(pair),str(capture),str(raster)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,text=True,capture_output=True,timeout=120)
  (a.output/f'{pair}-{capture}-{raster}.log').write_text(run.stdout+run.stderr)
  result=json.loads(next(x.removeprefix('CADENCE ') for x in run.stdout.splitlines() if x.startswith('CADENCE ')));results.append(result);print(json.dumps(result),flush=True)
 (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
