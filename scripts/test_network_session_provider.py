#!/usr/bin/env python3
"""Actual network helper/codec -> Watch provider -> CpuPort -> native async worker.
The upstream concurrency fixture supplies only deterministic SDK and outer host
boundaries. Sources are copied/compiled, never edited. No device/network access.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--watch',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();root=Path(__file__).resolve().parents[1];r=a.runtime.resolve();w=a.watch.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
source=r/'test/native_radio_async_test.cpp'
text=source.read_text()
marker='#include "WifiApi.h"'
assert text.count(marker)==1
text=text.replace(marker,marker+'''
extern "C" void network_session_ready(void);
extern "C" void network_session_end_service(void);
extern "C" void network_session_cancel(void);
extern "C" void network_session_borrow(const wifi_async_v1 *);
extern "C" void network_session_retained(void);
''')
marker=' if(test=="startup-unavailable"){'
assert text.count(marker)==1
text=text.replace(marker,''' if(test=="network-owned" || test=="network-retained"){
  app_workflow_start(api);app_workflow_scan();id=c.operation;assert(id);
  runStep();assert(!strcmp(reinterpret_cast<char*>(copied.sta.ssid),"Synthetic AP"));
  assert(!strcmp(reinterpret_cast<char*>(copied.sta.password),"synthetic-password"));
  assert(zero(&Async::request,sizeof(Async::request)) && credentialWipeObserved);
  associated=true;netif->up=true;ip.ip.addr=0x0504a8c0u;runStep();
  network_session_ready();assert(c.serviceLease && resourceOwner.heldReady());
  const auto count=calls.size();network_session_cancel();runStep();assert(calls.size()==count && c.operation==id);
  network_session_end_service();network_session_cancel();
  if(test=="network-retained")failure="disconnect";
  runStep();
  if(test=="network-retained"){
   network_session_retained();assert(c.closing && c.operation==id && !port.providerStorageSafe() && !port.appExitSafe());
   const auto retainedCount=calls.size();runStep();assert(calls.size()==retainedCount);
  }else{
   app_workflow_cleaned();assert(!c.operation && !c.serviceLease && port.appExitSafe() && Async::idle());
   assert(zero(&copied,sizeof(copied)));
  }
 }else if(test=="network-borrowed"){
  request.kind=RISC_RADIO_REQUEST_JOIN;strcpy(request.ssid,"Synthetic AP");strcpy(request.password,"synthetic-password");
  begin();runStep();associated=true;netif->up=true;ip.ip.addr=0x0504a8c0u;runStep();
  assert(poll()==RISC_RADIO_PENDING && progress.phase==RISC_RADIO_CONNECTED);
  const auto count=calls.size();network_session_borrow(api);
  assert(c.operation==id && c.active && !c.closing && calls.size()==count && !called("disconnect"));
  close();
 }else if(test=="startup-unavailable"){''')
(out/'network-provider-host.cpp').write_text(text)
san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
commands=[]
def run(command):
 commands.append([str(x) for x in command]);subprocess.run(command,check=True,timeout=180)
common=['-Wall','-Wextra','-Werror','-g',*san]
for source,name in [(root/'lib/PortableApps/src/PortableNetworkSession.c','session'),(root/'test/native_apps/network_session_provider_seam.c','seam')]:
 run([os.environ.get('CC','cc'),'-std=c11',*common,'-I'+str(root/'lib/PortableApps/include'),'-c',source,'-o',out/(name+'.o')])
run([os.environ.get('CC','cc'),'-std=c11',*common,'-I'+str(w/'include'),'-I'+str(w/'sdk/driver'),'-Dt5_driver_get=production_wifi_get','-c',w/'drivers/twatch_wifi/driver.c','-o',out/'wifi.o'])
inc=[r/'src',r/'sdk/app',r/'sdk/driver',r/'sdk/hardware',r/'lib/ArduinoJson/src',r/'test/drivers/stubs',r/'test/native_radio_shim',r/'test/native_radio_async_task_shim',r/'test',w/'include',w/'sdk/driver',w/'drivers/twatch_wifi']
sources=['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp','src/ports/esp32s3/CpuPort.cpp']
run([os.environ.get('CXX','c++'),'-std=c++17',*common,'-Wno-missing-field-initializers','-Wno-unused-function','-pthread','-DRISC_STAGE_LOGS=1','-DAPP_WORKFLOW_OBJECT','-DAPP_WORKFLOW_UNAVAILABLE','-rdynamic',*['-I'+str(x) for x in inc],*[r/x for x in sources],out/'network-provider-host.cpp',out/'wifi.o',out/'session.o',out/'seam.o','-ldl','-o',out/'network-provider'])
results=[]
for case in ['app-workflow','app-workflow-unavailable','network-owned','network-borrowed','network-retained']:
 result=subprocess.run([out/'network-provider',case],capture_output=True,text=True,timeout=30)
 (out/(case+'.log')).write_text(result.stdout+result.stderr)
 print(case,result.returncode,flush=True);results.append({'case':case,'returncode':result.returncode})
 if result.returncode:print(result.stdout+result.stderr)
 result.check_returncode()
(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
paths=[root/'lib/PortableApps/src/PortableNetworkSession.c',root/'lib/PortableApps/include/PortableNetworkSession.h',root/'lib/PortableApps/include/PortableWifiProfiles.h',root/'lib/PortableApps/include/PortableWifiCredentials.h',r/'test/native_radio_async_test.cpp',r/'src/ports/esp32s3/NativeRadioAsync.h',r/'src/ports/esp32s3/CpuRadioAsync.inc',w/'drivers/twatch_wifi/driver.c']
(out/'evidence.json').write_text(json.dumps({'results':results,'sanitized':a.sanitize,'leak_detection':os.environ.get('ASAN_OPTIONS','default'),'hardware':'not accessed','limits':'Deterministic SDK and synthetic outer KV/owner; no physical RF, NVS, or device qualification. Actual helper/profile codec/provider/CpuPort/native asynchronous worker executed.','source_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}},indent=2)+'\n')
