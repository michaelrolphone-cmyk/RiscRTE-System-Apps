#!/usr/bin/env python3
"""Actual Paper Clock + Files/USB drawer behavior through production Runtime."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import portable_native_toolbar_build as native
from test_resident_system_clients import CAPS
ROOT=Path(__file__).resolve().parents[1]
CASES=['child-home-closed','child-home-open','host-frontlight','child-frontlight',
       'host-clean','child-clean','host-no-clean','child-no-clean','host-usb','child-usb',
       'host-clean-audio','host-usb-audio','host-frontlight-audio','host-clean-submit-refused','child-clean-status-refused','host-clean-wait-refused','host-clean-failed-live','child-clean-superseded-live']
CASES += ['host-usb-repeat','host-usb-repeat-audio','host-usb-cancel-then-open','child-usb-cancel-then-open','host-usb-old-row']
TONE_CASES=['host-tone','child-tone','host-tone-audio','host-tone-absent','host-tone-unavailable','host-tone-failed-get','host-tone-failed-set','host-tone-failed-open-get','host-tone-failed-open-set']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--display-sdk',type=Path,required=True);p.add_argument('--alarm-sdk',type=Path,required=True);p.add_argument('--msc-sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/shared-quick-reference');p.add_argument('--normal-only',action='store_true');p.add_argument('--case',action='append',choices=CASES+TONE_CASES);p.add_argument('--tone-sdk',type=Path);p.add_argument('--stage-logs',action='store_true');p.add_argument('--snapshot',action='store_true');a=p.parse_args()
 out=a.output_dir.resolve();inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
 for name in ['RiscRuntimeV1.h','RiscResidentShellV1.h','RiscRealtimeV1.h']:shutil.copyfile(a.runtime/'sdk/app'/name,inc/name)
 if (a.runtime/'sdk/app/RiscFailureEvidenceV1.h').is_file():shutil.copyfile(a.runtime/'sdk/app/RiscFailureEvidenceV1.h',inc/'RiscFailureEvidenceV1.h')
 for name in ['RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h']:shutil.copyfile(a.display_sdk/name,inc/name)
 for name in ['AlarmServiceV1.h','AlarmServiceV2.h']:shutil.copyfile(a.alarm_sdk/name,inc/name)
 shutil.copyfile(a.msc_sdk/'RiscUsbDeviceMscV1.h',inc/'RiscUsbDeviceMscV1.h')
 production=[*ROOT.glob('Apps/*.inc'),*[ROOT/'Apps'/n for n in ['paper_clock.c','file_browser.c','usb_sd_transfer.c']]]
 production += [p for directory in ['lib/PortableApps','lib/NativeApps/include'] for p in (ROOT/directory).rglob('*') if p.is_file()]
 if a.tone_sdk:shutil.copyfile(a.tone_sdk/'RiscDisplayOutputFrontlightV1.h',inc/'RiscDisplayOutputFrontlightV1.h')
 source_inputs={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(production))}
 sdk_inputs={str(p.relative_to(inc)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inc.rglob('*')) if p.is_file()}
 runtime_revision=subprocess.check_output(['git','-C',str(a.runtime),'rev-parse','HEAD'],text=True).strip()
 results=[]
 for san in ([False] if a.normal_only else [False,True]):
  build=out/('sanitized' if san else 'normal');build.mkdir(parents=True,exist_ok=True)
  sanitizers=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if san else []
  base=['-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-misleading-indentation',*sanitizers,'-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include')]
  common=['-DREFERENCE_RUNTIME_FIXTURE','-DPORTABLE_PAPER_PREFERENCES','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_CLIENT','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_RESIDENT_POLICY']
  if a.stage_logs:common+=['-DPORTABLE_STAGE_LOGS']
  if a.snapshot:common+=['-DPORTABLE_RASTER_SNAPSHOT']
  for name in ['host','client','usb_sd_transfer']:
   flags=list(common);sources=[ROOT/'test/native_apps/shared_quick_app.c',ROOT/'test/native_apps/shared_quick_adapter.c']
   if name=='host':
    flags+=['-DPORTABLE_FRONTLIGHT_TONE'] if a.tone_sdk else []
    flags+=['-DPORTABLE_RESIDENT_SHELL_HOST','-DPORTABLE_RTC_WALL_TIME','-DPAPER_CLOCK_LAUNCHER="client.elf"','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_QUICK_USB_TRANSFER','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_X4_IDLE_POLICY','-DPORTABLE_LOW_BATTERY']
    sources += [ROOT/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_session.c','quick_render.c','quick_radios.c']]
   elif name=='client':
    flags+=['-DPORTABLE_RESIDENT_SHELL_CLIENT','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_FILE_BROWSER_APP','-DPORTABLE_FILE_BROWSER_CAPABILITY="storage.volume"','-DPORTABLE_FILE_BROWSER_INSTANCE=9u','-DPORTABLE_FILE_BROWSER_HANDLERS','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_PAPER_TRANSITIONS']
    sources += [ROOT/s for s in native.SOURCES]
   else:flags+=['-DPORTABLE_RESIDENT_SHELL_CLIENT','-DPORTABLE_USB_TRANSFER_APP','-DPORTABLE_APP_LAUNCH_GUARD']
   subprocess.run(['cc','-std=c11',*base,'-fPIC','-shared','-Wl,-Bsymbolic',*flags,*map(str,sources),'-o',str(build/(name+'.elf'))],check=True)
   symbols=subprocess.check_output(['nm',str(build/(name+'.elf'))],text=True);assert (' pqa_render' in symbols)==(name=='host')
  subprocess.run(['cc','-std=c11',*base,'-fPIC','-shared',str(ROOT/'test/native_apps/resident_system_file_receiver.c'),'-o',str(build/'receiver.elf')],check=True)
  fixture=ROOT/'test/native_apps/shared_quick_reference_test.c'
  for index,(cap,version) in enumerate(CAPS,1):subprocess.run(['cc','-std=c11',*base,'-I'+str(a.runtime/'sdk/driver'),'-fPIC','-shared','-DPOLICY_PROVIDER='+str(index),'-DPOLICY_CAPABILITY="'+cap+'"','-DPOLICY_API='+str(version),str(fixture),'-o',str(build/('provider-'+str(index)+'.elf'))],check=True)
  subprocess.run(['cc','-std=c11',*base,*(['-DTEST_STAGE_LOGS'] if a.stage_logs else []),*(['-DPORTABLE_RASTER_SNAPSHOT'] if a.snapshot else []),*(['-DPORTABLE_FRONTLIGHT_TONE'] if a.tone_sdk else []),'-DPORTABLE_ALARM_CLIENT','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_X4_IDLE_POLICY','-c',str(fixture),'-o',str(build/'fixture.o')],check=True)
  rsources=[a.runtime/s for s in ['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp']]
  rincs=['-I'+str(a.runtime/s) for s in ['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test/drivers/stubs']]
  binary=build/'reference-test'
  subprocess.run(['c++','-std=c++17','-g','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*sanitizers,*rincs,'-rdynamic',*(['-no-pie'] if san else []),*map(str,rsources),str(ROOT/'test/native_apps/shared_quick_runtime.cpp'),str(build/'fixture.o'),'-ldl','-o',str(binary)],check=True)
  for case in a.case or CASES+(TONE_CASES if a.tone_sdk else []):
   capture=build/'captures'/case;capture.mkdir(parents=True,exist_ok=True)
   r=subprocess.run([binary,build,case],check=True,text=True,stdout=subprocess.PIPE,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',REFERENCE_CAPTURE_DIR=str(capture)));print(('sanitized' if san else 'normal'),r.stdout.strip(),flush=True);results.append({'sanitized':san,'case':case,'result':r.stdout.strip(),'completed_frames':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(capture.glob('*.pbm'))}})
   try:
    from PIL import Image
    for image in capture.glob('*.pbm'):Image.open(image).save(image.with_suffix('.png'))
   except ImportError:pass
 assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest for p,digest in source_inputs.items()),'Production input changed during matrix'
 (out/'evidence.json').write_text(json.dumps({'runs':results,'process_cases':len(results),'runtime_mocked':False,'hardware_tested':False,'runtime_revision':runtime_revision,'runtime_source_sha256':{str(p.relative_to(a.runtime)):hashlib.sha256(p.read_bytes()).hexdigest() for p in rsources},'production_source_sha256':source_inputs,'staged_sdk_sha256':sdk_inputs,'base_compile_flags':base,'common_app_defines':common,'sources':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__).resolve(),ROOT/'scripts/test_resident_system_clients.py',ROOT/'scripts/portable_native_toolbar_build.py',ROOT/'test/native_apps/resident_system_test.c',ROOT/'test/native_apps/resident_system_file_receiver.c',*ROOT.glob('test/native_apps/shared_quick_*')] if p.is_file()}},indent=2)+'\n')
if __name__=='__main__':main()
