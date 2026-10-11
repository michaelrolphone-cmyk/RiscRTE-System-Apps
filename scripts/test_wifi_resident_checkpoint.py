#!/usr/bin/env python3
"""Wi-Fi controller/resident seam plus actual provider/CpuPort/native worker.
The resident dispatch, peripherals and SDK boundary are synthetic; no hardware.
"""
import argparse, hashlib, json, os, platform, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--watch',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];r=a.runtime.resolve();w=a.watch.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
commands=[];results=[]
def run(cmd,log=None):
 cmd=list(map(str,cmd));commands.append(cmd)
 x=subprocess.run(cmd,capture_output=True,text=True,timeout=120)
 if log:log.write_text(x.stdout+x.stderr)
 if x.returncode:print(x.stdout+x.stderr);x.check_returncode()
 return x.stdout
for mode in ('normal','ubsan'):
 d=out/mode;d.mkdir(exist_ok=True)
 san=[] if mode=='normal' else ['-fsanitize=undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 flags=['-Wall','-Wextra','-Werror','-Wno-unused-function',*san]
 inc=['-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),'-I'+str(r/'sdk/app')]
 c=[os.environ.get('CC','cc'),'-std=c11','-DTEST_IDLE_ELIGIBILITY',*flags,*inc]
 run([*c,root/'test/native_apps/wifi_resident_checkpoint_test.c','-o',d/'resident'])
 run([d/'resident'],d/'resident.log')
 run([d/'resident','retained'],d/'retained.log')
 run([*c,'-DTEST_WIFI_RESIDENT_CHECKPOINT','-Drisc_runtime_get_api=app_fixture_runtime_get_api','-c',root/'test/native_apps/wifi_async_app_seam.c','-o',d/'app.o'])
 run([os.environ.get('CC','cc'),'-std=c11',*flags,'-I'+str(w/'include'),'-I'+str(w/'sdk/driver'),'-Dt5_driver_get=production_wifi_get','-c',w/'drivers/twatch_wifi/driver.c','-o',d/'wifi.o'])
 ri=['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test/drivers/stubs','test/native_radio_shim','test/native_radio_async_task_shim']
 sources=['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp','src/ports/esp32s3/CpuPort.cpp','test/native_radio_async_test.cpp']
 host=['-Wno-unused-private-field','-Wno-unused-but-set-variable'] if platform.system()=='Darwin' else []
 run([os.environ.get('CXX','c++'),'-std=c++17','-g','-pthread','-rdynamic',*flags,'-Wno-missing-field-initializers',*host,'-DRISC_STAGE_LOGS=1','-DAPP_WORKFLOW_OBJECT',*['-I'+str(r/x) for x in ri],'-I'+str(w/'include'),'-I'+str(w/'sdk/driver'),'-I'+str(w/'drivers/twatch_wifi'),*[r/x for x in sources],d/'wifi.o',d/'app.o','-ldl','-o',d/'native'])
 run([d/'native','app-workflow'],d/'native.log')
 results.append({'mode':mode,'resident':'PASS','native_app_workflow':'PASS'});print(mode,'PASS',flush=True)
files=[root/'lib/PortableApps/src/resident_adapter.inc',root/'Apps/wifi_settings_portable.inc',root/'test/native_apps/wifi_resident_seam.inc',root/'test/native_apps/wifi_resident_checkpoint_test.c',root/'test/native_apps/wifi_async_app_seam.c',r/'src/ports/esp32s3/NativeRadioAsync.h',r/'src/ports/esp32s3/CpuRadioAsync.inc',w/'drivers/twatch_wifi/driver.c']
(out/'receipt.json').write_text(json.dumps({'results':results,'commands':commands,'inputs':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'hardware_tested':False,'limitation':'Actual controller/checkpoint/provider/CpuPort/worker, synthetic resident dispatch and SDK/peripherals. Native worker gates wifi_init; never a real RF scan.'},indent=2)+'\n')
