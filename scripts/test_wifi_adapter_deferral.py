#!/usr/bin/env python3
"""Source-bound shared adapter deferral tests; copied fakes only, no hardware."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--sdk-include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--terminal',action='store_true');p.add_argument('--sharing',action='store_true',help='Exercise Files sharing through the lifecycle-only adapter selector');a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=False)
cases=['kv-busy','time-cache','time-cold','time-invalid-busy','broadcast','alarm-busy','alarm-connected','alarm-retained','quick-action','quick-open','quick-handoff','home','return','battery-low','battery-high','battery-rearm']
helpers=['PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']
flags=['PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE','ALARM_SERVICE_TAGGED_V2','TEST_NATIVE_TOOLBAR_QUICK','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_ALARM_CLIENT','PORTABLE_INPUT_NAVIGATION','PORTABLE_WIFI_SETTINGS_APP','PORTABLE_WIFI_INSTANCE=15u','PORTABLE_LOW_BATTERY','PORTABLE_BLE_BROADCAST','PORTABLE_BLE_BROADCAST_DEFAULT_OFF','PORTABLE_HOME_APP="default.elf"','PORTABLE_RETURN_APP="parent.elf"']
if a.terminal:flags.append('PORTABLE_ALARM_TERMINAL_RETENTION')
if a.sharing:
 flags.remove('PORTABLE_WIFI_SETTINGS_APP')
 flags += ['TEST_FILE_SHARING_ADAPTER','PORTABLE_FILE_BROWSER_APP','PORTABLE_FILE_SHARING','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_CROWN_SLEEP_LOCAL']
 if not a.terminal:flags.append('PORTABLE_ALARM_TERMINAL_RETENTION')
 cases += ['sharing-idle','sharing-borrowed-quick','sharing-sleep','sharing-sleep-retained','sharing-home-retained','sharing-quick-retained','sharing-battery-retained','sharing-fini','sharing-fini-retained']
commands=[]
with tempfile.TemporaryDirectory(prefix='wifi-adapter-sdk-') as td:
 inc=Path(td)/'include';shutil.copytree(root/'lib/PortableApps/include',inc);shutil.copytree(root/'lib/PortableApps/time',Path(td)/'time')
 for name in ['AlarmServiceV1.h','AlarmServiceV2.h','RiscRealtimeV1.h']:shutil.copyfile(a.sdk_include/name,inc/name)
 binary=a.output/'wifi-adapter-deferral'
 cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []),*['-D'+f for f in flags],'-I'+str(inc),'-I'+str(root/'lib/NativeApps/include'),str(root/'test/native_apps/wifi_adapter_deferral_test.c'),*[str(root/'lib/PortableApps/src'/h) for h in helpers],'-Wl,--wrap=free','-o',str(binary)]
 commands.append(cmd);subprocess.run(cmd,check=True)
 for case in cases:
  cmd=[str(binary),case];commands.append(cmd);r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
  (a.output/(case+'.log')).write_text(r.stdout);print(r.stdout,end='');r.check_returncode()
binary=a.output/'wifi-alarm-failure-drain'
cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-DTEST_IDLE_ELIGIBILITY',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []),'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'test/native_apps/wifi_alarm_failure_drain_test.c'),'-o',str(binary)]
commands.append(cmd);subprocess.run(cmd,check=True)
cmd=[str(binary)];commands.append(cmd);r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
(a.output/'alarm-failure-drain.log').write_text(r.stdout);print(r.stdout,end='');r.check_returncode();cases.append('alarm-failure-drain')
tracked=[root/'test/native_apps/wifi_adapter_deferral_test.c',root/'test/native_apps/wifi_alarm_failure_drain_test.c',root/'test/native_apps/wifi_workflow_test.c',root/'test/native_apps/portable_wifi_test.c',*[root/'test/native_apps'/n for n in ['native_system_apps_test.c','portable_native_time_source_test.c','portable_native_toolbar_test.c','broadcast_fixture.h']],root/'Apps/wifi_settings_portable.inc',Path(__file__),*[root/'lib/PortableApps/src'/n for n in ['adapter.c','quick_adapter.inc','alarm.inc','broadcast_adapter.inc','resident_adapter.inc','native_custody_adapter.inc',*helpers]]]
(a.output/'evidence.json').write_text(json.dumps({'cases':cases,'sanitized':a.sanitize,'terminal_alarm':a.terminal or a.sharing,'files_sharing':a.sharing,'boundary':'Production shared adapter with copied network and sharing lifecycle doubles; actual HTTP/frontend and resident Runtime integration are separate tests.','sdk_sha256':{n:hashlib.sha256((a.sdk_include/n).read_bytes()).hexdigest() for n in ['AlarmServiceV1.h','AlarmServiceV2.h','RiscRealtimeV1.h']},'commands':commands,'source_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked},'hardware':'not run','publication':'none'},indent=2)+'\n')
