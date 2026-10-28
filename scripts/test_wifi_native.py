#!/usr/bin/env python3
"""Native Wi-Fi presentation and real local controller; fake providers only."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'test/native_apps/portable_wifi_test.c').read_text()
s=s.replace('#include "../../lib/PortableApps/src/adapter.c"','#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c'))).replace('#include "../../Apps/wifi_settings.c"','#include '+json.dumps(str(ROOT/'Apps/wifi_settings.c')))
s=s.replace('pixels[240*240]','pixels[WIFI_TEST_W*WIFI_TEST_H]').replace('.width=240,.height=240','.width=WIFI_TEST_W,.height=WIFI_TEST_H').replace('.stride_bytes=480,','.stride_bytes=WIFI_TEST_W*2,').replace('script[64]','script[512]').replace('script_count<64','script_count<512')
fixture=out/'fixture.c';fixture.write_text(s);runs=[]
for invalid in [['--nova-ui'],['--display-rotation','90'],['--touch-rotation','180'],['--shared-text-input'],['--paper-transitions'],['--time-profile','watch-native-time']]:
 cmd=['python',str(ROOT/'scripts/build_portable_wifi.py'),'--watch-native-ui',*invalid]
 run=subprocess.run(cmd,text=True,capture_output=True);assert run.returncode==2 and '--watch-native-ui requires' in run.stderr
 runs.append({'name':'rejected '+' '.join(invalid),'command':cmd,'result':'PASS'})
common=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
for sanitized in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 for name in ['layout','render','controller']:
  for w,h in ([(240,240),(328,328),(327,411),(502,410)] if name=='controller' else [(0,0)]):
   label=f'{name}-{w}x{h}'+('-san' if sanitized else '');exe=out/label
   extra=[]
   if name=='controller':extra=['-D'+x for x in ['PORTABLE_WIFI_NATIVE_PRESENTATION','PORTABLE_NOVA_UI','PORTABLE_RF_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_WATCH_SAFE_INSETS','PORTABLE_SAFE_VIEWPORT']]+[f'-DWIFI_TEST_W={w}',f'-DWIFI_TEST_H={h}','-DWIFI_NATIVE_FIXTURE='+json.dumps(str(fixture))]
   cmd=[*common,*flags,*extra,str(ROOT/'test/native_apps'/f'wifi_native_{name}_test.c'),'-o',str(exe)]
   subprocess.run(cmd,check=True);captures=out/'captures';captures.mkdir(exist_ok=True)
   run=subprocess.run([str(exe),*([str(captures)] if name=='render' else [])],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':os.environ.get('ASAN_OPTIONS','detect_leaks=0'),'UBSAN_OPTIONS':'halt_on_error=1'})
   (out/(label+'.log')).write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='',flush=True);run.check_returncode();runs.append({'name':label,'command':cmd,'result':run.stdout.strip()})
# Exercise the real adapter command-span replay as well as the primitive fixture.
rf=(ROOT/'test/native_apps/portable_handoff_test.c').read_text().replace('#include "../../lib/PortableApps/src/adapter.c"','#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c')))
rf=rf.replace('mock_pixels[240*243]','mock_pixels[RF_TEST_H*(RF_TEST_W+3)]').replace('.width=240,.height=240','.width=RF_TEST_W,.height=RF_TEST_H').replace('.stride_bytes=486,','.stride_bytes=(RF_TEST_W+3)*2,').replace('.flags=mock_flags,','.flags=mock_flags,.safe_insets={.left=RF_INSET,.right=RF_INSET,.top=RF_INSET,.bottom=RF_INSET},')
rf_fixture=out/'rf-safe-fixture.c';rf_fixture.write_text(rf)
for w,h,inset in [(466,466,69),(410,502,17)]:
 for sanitized in (False,True):
  label=f'physical-replay-{w}x{h}'+('-san' if sanitized else '');exe=out/label
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  cmd=[*common,*flags,*['-D'+v for v in ['PORTABLE_RF_NATIVE_CANVAS','PORTABLE_NOVA_UI','PORTABLE_RASTER_SNAPSHOT','PORTABLE_RETAINED_RGB565_HANDOFF','PORTABLE_WATCH_SAFE_INSETS','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_SAFE_VIEWPORT']],f'-DRF_TEST_W={w}',f'-DRF_TEST_H={h}',f'-DRF_INSET={inset}','-DFRAME_FIXTURE='+json.dumps(str(rf_fixture)),str(ROOT/'test/native_apps/rf_native_raster_test.c'),'-o',str(exe)]
  subprocess.run(cmd,check=True);run=subprocess.run([str(exe)],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':os.environ.get('ASAN_OPTIONS','detect_leaks=0'),'UBSAN_OPTIONS':'halt_on_error=1'})
  (out/(label+'.log')).write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='',flush=True);run.check_returncode();runs.append({'name':label,'command':cmd,'result':run.stdout.strip()})
(out/'receipt.json').write_text(json.dumps({'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'runs':runs,'hardware_verified':False},indent=2)+'\n')
