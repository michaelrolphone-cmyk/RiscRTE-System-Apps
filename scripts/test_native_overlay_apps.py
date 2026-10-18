#!/usr/bin/env python3
"""Actual Settings/Wi-Fi native overlays with independent bounded providers.

The unchanged target recipes qualify full deployment composition separately.
This host suite selects production app, persistence and overlay controller paths;
only provider fixtures are staged. It does not emulate hardware or bridge code.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
from test_native_settings import stage_fixture
from native_overlay_evidence import source_paths
ROOT = Path(__file__).resolve().parents[1]
CASES = {0:'quick-draft-restoration',1:'paired-atomic-preferences-and-reentry',2:'alarm-interrupts-quick',3:'alarm-interrupts-torch',4:'queued-stale-and-fresh-app-contact'}
RUNTIME_PIN = '48229bd1593a2e24e95bf4537b495ee615990fd3'
COMMON = ['PORTABLE_NATIVE_CANVAS','PORTABLE_NOVA_UI','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_CHECKED_STORAGE_RUNTIME','PORTABLE_ALARM_TERMINAL_RETENTION','PORTABLE_QUICK_NATIVE','PORTABLE_ALARM_NATIVE','PORTABLE_QUICK_RADIOS','PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_ASYNC_RGB565','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_WATCH_NATIVE_HEIGHT']
OMITTED = {
 'PORTABLE_INPUT_NAVIGATION_LOCAL':'Host fixture supplies bounded input.navigation rather than the physical power-key bridge.',
 'PORTABLE_BLE_BROADCAST':'No telemetry broadcast provider in this app-overlay seam.',
 'PORTABLE_CONTEXTS_CLIENT':'No contexts service provider in this app-overlay seam.',
 'PORTABLE_RUNTIME_APP_CATALOG':'Fixture supplies inert local catalog; target ELF covers runtime catalog composition.',
 'PORTABLE_MOTION_WAKE':'No motion/physical wake provider; no physical sleep or wake claim.',
 'WATCH_ALARM_SLEEP_RESUME':'No bridge sleep-resume implementation; inherited sleep entry is a bounded fake.',
 'PORTABLE_LOW_BATTERY':'No physical battery or low-battery workflow in this overlay seam.',
 'PORTABLE_RTC_UTC8_DENVER':'No deployment timezone conversion in this overlay seam.',
 'PORTABLE_NAVIGATION_RETENTION':'Navigation bridge is replaced by the fixture capability provider.',
 'PORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL':'RTC-changing commands are outside the overlay cases.',
 'PORTABLE_TAP_SETTINGS':'Tap-wake preference is outside the overlay cases.',
}
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--app',choices=('settings','wifi'));p.add_argument('--size');p.add_argument('--case',type=int,choices=CASES,action='append');p.add_argument('--snapshot',type=int,choices=(0,1));p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);staged=out/'fixtures';staged.mkdir()
 transformations=[]
 sdk=out/'runtime-sdk';sdk.mkdir()
 runtime_paths=subprocess.check_output(['git','-C',str(a.runtime.resolve()),'ls-tree','-r','--name-only',RUNTIME_PIN,'sdk/app'],text=True).splitlines()
 for name in runtime_paths:
  if name.endswith('.h'):(sdk/Path(name).name).write_bytes(subprocess.check_output(['git','-C',str(a.runtime.resolve()),'show',RUNTIME_PIN+':'+name]))
 include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include)
 shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
 for header in ('RiscRuntimeV1.h','RiscKeyValueAtomicV1.h'):shutil.copyfile(sdk/header,include/header)

 def replace(s,before,after,path):
  n=s.count(before)
  if not n: raise ValueError('Fixture shape absent: '+str(path)+': '+before)
  transformations.append({'fixture':str(path),'before':before,'after':after,'occurrences':n});return s.replace(before,after)
 base=stage_fixture(staged,'portable_settings_test.c')
 transformations.append({'fixture':'portable_settings_test.c','operation':'test_native_settings.stage_fixture: dimensioned provider framebuffer/stride/display/raw coordinates; remove legacy coordinate translation'})
 alarm=staged/'portable_alarm_test.c';alarm.write_text(replace((ROOT/'test/native_apps/portable_alarm_test.c').read_text(),'#include "portable_settings_test.c"',f'#include "{base}"','portable_alarm_test.c'))
 quick=staged/'quick_adapter_test.c';s=(ROOT/'test/native_apps/quick_adapter_test.c').read_text();s=replace(s,'#include "portable_alarm_test.c"',f'#include "{alarm}"',quick);s=replace(s,'sizeof(framebuffer)','test_size',quick);quick.write_text(s)
 names=('portable_wifi_test.c','wifi_workflow_test.c','wifi_atomic_workflow_test.c','wifi_availability_test.c')
 for name in names:
  s=(ROOT/'test/native_apps'/name).read_text()
  for dep in names:
   if '"'+dep+'"' in s:s=replace(s,'"'+dep+'"','"'+str(staged/dep)+'"',name)
  for dep in ('lib/PortableApps/src/adapter.c','Apps/wifi_settings.c'):
   if '"../../'+dep+'"' in s:s=replace(s,'"../../'+dep+'"','"'+str(ROOT/dep)+'"',name)
  if name=='portable_wifi_test.c':
   s=replace(s,'static uint16_t pixels[TEST_WIFI_RGB_WIDTH*TEST_WIFI_RGB_HEIGHT];','static uint16_t *pixels;',name)
   s=replace(s,'.stride_bytes=TEST_WIFI_RGB_WIDTH*2,.size_bytes=sizeof(pixels)', '.stride_bytes=ov_stride,.size_bytes=ov_bytes',name)
   s=replace(s,'.acquire=fake_acquire,.release=fake_release', '.acquire=ov_acquire,.release=ov_release',name)
   s=replace(s,'h->uptime_ms=ticks;return poll_count<limit_polls;', 'assert(ticks<120000);h->uptime_ms=ticks;return true;',name)
  if name=='wifi_workflow_test.c':s=replace(s,'runtime_api.acquire=workflow_acquire;runtime_api.release=workflow_release;', 'runtime_api.acquire=ov_acquire;runtime_api.release=ov_release;',name)
  (staged/name).write_text(s)
 # Use the validator's explicit fixture closure. A separately owned app fixture
 # is not an input to Settings/Wi-Fi merely because its filename shares a prefix.
 hashes=source_paths(ROOT)
 receipt={'scope':'Actual production Settings/Wi-Fi controllers and generic native Quick/alarm paths with bounded host capability providers. Target builds separately qualify all deployment flags and bridges.','hardware_tested':False,'cases':CASES,'source_sha256':hashes,'runtime_pin':RUNTIME_PIN,'runtime_sdk_sha256':{p.name:sha(p) for p in sdk.glob('*.h')},'staged_include_sha256':{p.name:sha(p) for p in include.glob('*.h')},'fixture_sha256':{p.name:sha(p) for p in staged.glob('*.c')},'transformations':transformations,'omitted_provider_groups':OMITTED,'builds':[],'runs':[],'status':'RUNNING'}
 def save(): (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 save();sizes=[tuple(map(int,a.size.split('x')))] if a.size else [(240,240),(410,502)]
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
 binary=None
 try:
  for app in ([a.app] if a.app else ['settings','wifi']):
   for w,h in sizes:
    for snapshot in ([a.snapshot] if a.snapshot is not None else [0,1]):
     name=f'{app}-{w}x{h}-snapshot{snapshot}-san{int(a.sanitize)}';binary=out/name
     defines=COMMON+(['PORTABLE_SETTINGS_NATIVE_UI','PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL','PORTABLE_SLEEP_SETTINGS','PORTABLE_ALARM_SETTINGS'] if app=='settings' else ['PORTABLE_WIFI_NATIVE_UI','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_QUICK_ACTIONS','PORTABLE_HOME_AUTOMATIC_PREFERENCES','PORTABLE_WIFI_STORAGE_INSTANCE=6'])
     flags=['-D'+d for d in defines]+(['-DPORTABLE_RASTER_SNAPSHOT'] if snapshot else [])
     flags += [('-DNATIVE_OVERLAY_SETTINGS_FIXTURE=' if app=='settings' else '-DNATIVE_OVERLAY_WIFI_FIXTURE=')+json.dumps(str(quick if app=='settings' else staged/'wifi_availability_test.c'))]
     if app=='wifi':flags += [f'-DTEST_WIFI_RGB_WIDTH={w}',f'-DTEST_WIFI_RGB_HEIGHT={h}']
     if a.sanitize:flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
     sources=([ROOT/'Apps/settings.c'] if app=='settings' else [])+[ROOT/f'test/native_apps/native_overlay_{app}_test.c']
     sources += [ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]
     if app=='wifi':sources += [ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c']
     cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g0','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-I'+str(include),'-I'+str(sdk),*['-I'+str(ROOT/n) for n in ('lib/NativeApps/include','lib/RemoteFiles/vendor/tinydtls_sha2')],*map(str,sources),'-o',str(binary)]
     build={'name':name,'command':cmd,'selected_defines':defines,'snapshot':snapshot,'sanitized':a.sanitize};receipt['builds'].append(build);save()
     r=subprocess.run(cmd,capture_output=True,text=True,timeout=180);log=out/(name+'-compile.log');log.write_text(r.stdout+r.stderr);build.update(returncode=r.returncode,log_sha256=sha(log));save()
     if r.returncode:print(log.read_text(),flush=True);r.check_returncode()
     build['binary_sha256']=sha(binary)
     pre=cmd[:cmd.index(str(sources[0]))]+['-dM','-E',str(ROOT/f'test/native_apps/native_overlay_{app}_test.c')]
     macros_text=subprocess.check_output(pre,text=True,timeout=120);macro_file=out/(name+'-macros.txt');macro_file.write_text(macros_text)
     macros=dict(re.findall(r'^#define (\w+)(?: (.*))?$',macros_text,re.M))
     required=COMMON+['PORTABLE_QUICK_ACTIONS','PORTABLE_ALARM_CLIENT']+(['PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL'] if app=='settings' else ['PORTABLE_WIFI_ATOMIC_EXPERIMENTAL','PORTABLE_WIFI_REMEMBER_CONNECTED','PORTABLE_SETTINGS_AVAILABILITY','PORTABLE_HOME_AUTOMATIC_PREFERENCES'])
     assert all(n in macros for n in required),'Missing selected app/overlay feature'
     build['preprocessor_command']=pre;build['macro_sha256']=sha(macro_file);build['actual_macros']={n:v for n,v in macros.items() if n.startswith(('PORTABLE_','WATCH_'))}

     dep=subprocess.run([cmd[0],'-MM',*cmd[1:cmd.index('-o')]],capture_output=True,text=True,check=True)
     deps={}
     for token in shlex.split(dep.stdout.replace('\\\n',' ')):
      if token.endswith(':'):continue
      path=Path(token).resolve();deps[str(path)]=sha(path)
     build['compiler_dependency_sha256']=deps
     for case in a.case or CASES:
      args=[str(binary),*([str(w),str(h)] if app=='settings' else []),str(case)]
      r=subprocess.run(args,capture_output=True,text=True,timeout=30,env=env);log=out/(name+f'-case{case}.log');log.write_text(r.stdout+r.stderr)
      receipt['runs'].append({'build':name,'case':case,'command':args,'returncode':r.returncode,'log_sha256':sha(log)});save();print(name,case,r.stdout+r.stderr,flush=True);r.check_returncode()
     binary.unlink()
  assert hashes==source_paths(ROOT),'Input sources changed during run'
  receipt['status']='PASS'
 except Exception as e:receipt['status']='FAIL';receipt['error']=str(e);raise
 finally:
  if binary is not None and binary.exists():
   receipt['disposable_binary_sha256']=sha(binary);binary.unlink()
  save()
if __name__=='__main__':main()
