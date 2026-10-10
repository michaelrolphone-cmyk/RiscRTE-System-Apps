#!/usr/bin/env python3
"""Current production Wi-Fi path with deterministic radio/display boundary faults.
No devices, real networks, credentials, publication, or production-file writes.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--watch',type=Path,required=True)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--expect-fixed',action='store_true',help='Require one status query and accept only confirmed COMPLETE for aged frames')
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
fixture=Path(__file__).resolve().parents[1]/'test/wifi_scan_latency'
shutil.copytree(a.system/'lib/PortableApps/include',out/'include',dirs_exist_ok=True)
shutil.copytree(a.system/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for n in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:shutil.copyfile(a.runtime/'sdk/app'/n,out/'include'/n)
for n in ['AlarmServiceV1.h','AlarmServiceV2.h']:shutil.copyfile(a.sdk/n,out/'include'/n)
(out/'public').mkdir(exist_ok=True)
for n in ['WifiApi.h','RiscRadioAsyncV1.h']:
 if (out/'include'/n).exists():shutil.copyfile(out/'include'/n,out/'public'/n)
for n in ['app-cross.c','app-entry.c','cross-host.cpp','native_wifi_sdk_fixture.inc']:shutil.copyfile(fixture/n,out/n)
commands=json.loads((fixture/'commands.json').read_text())
replacements={'{SYSTEM}':str(a.system.resolve()),'{RUNTIME}':str(a.runtime.resolve()),'{WATCH}':str(a.watch.resolve()),'{OUTPUT}':str(out)}
actual=[]
for cmd in commands:
 for old,new in replacements.items():cmd=[arg.replace(old,new) for arg in cmd]
 if a.sanitize:cmd+=['-O0','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 actual.append(cmd);subprocess.run(cmd,check=True)
(out/'commands.json').write_text(json.dumps(actual,indent=2)+'\n')
cases=['cold-navigation','sdk-init-failure','allocation-failure','iq-lease','cleanup-failure','telemetry-failure','bluetooth-fault','prior-wifi-lease','policy-io','policy-corrupt','policy-off','scan-repeated','scan-timeout','scan-cleanup-retained','pending-display-scan','pending-display-delayed-complete','pending-display-delayed-active','pending-display-delayed-callback-failure','pending-display-delayed-failed','pending-display-delayed-superseded']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');env.pop('WIFI_EXPECT_AGED_COMPLETE',None)
if a.expect_fixed:env['WIFI_EXPECT_AGED_COMPLETE']='1'
results=[]
for case in cases:
 result=subprocess.run([out/'wifi-cross-layer',case],capture_output=True,text=True,timeout=30,env=env)
 (out/(case+'.log')).write_text(result.stdout+result.stderr);results.append({'case':case,'returncode':result.returncode});print(case,result.returncode,flush=True)
 if result.returncode:print((result.stdout+result.stderr)[-1800:]);result.check_returncode()
paths=[a.system/'Apps/wifi_settings_portable.inc',a.system/'lib/PortableApps/src/adapter.c',a.system/'lib/PortableApps/src/native_custody_adapter.inc',a.runtime/'src/ports/esp32s3/NativeRadio.h',a.runtime/'src/ports/esp32s3/CpuPort.cpp',a.runtime/'src/bootstrap/Runtime.cpp',a.watch/'drivers/twatch_wifi/driver.c',a.watch/'drivers/twatch_ble/driver.c']
(out/'evidence.json').write_text(json.dumps({'results':results,'sanitized':a.sanitize,'leak_sanitizer':False,'expect_fixed_aged_complete':a.expect_fixed,'hardware':'not accessed','fixture_sha256':{str(x.relative_to(fixture)):hashlib.sha256(x.read_bytes()).hexdigest() for x in fixture.iterdir() if x.is_file()},'limit':'12-second SDK delay is injected; no claim of physical incident causality. Production app/adapter/providers/CpuPort/NativeRadio execute; outer grants, display/time/storage and SDK calls are synthetic.','sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}},indent=2)+'\n')
