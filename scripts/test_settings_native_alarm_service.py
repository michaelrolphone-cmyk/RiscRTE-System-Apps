#!/usr/bin/env python3
"""Real native Settings controller paired with actual editor-aware service."""
import argparse,pathlib,subprocess,tempfile,os,json,hashlib,shutil
from build_portable_settings import native_time_sdk,stage_native_time_sdk
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=pathlib.Path);p.add_argument('--utilities',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
class Options: native_time_runtime_repo=a.runtime
runs=[];commands=[]
with tempfile.TemporaryDirectory() as tmp:
 out=pathlib.Path(tmp);inc=stage_native_time_sdk(out,native_time_sdk(Options,p));source=root/'test/native_apps/settings_native_alarm_service_test.c'
 for name in ['AlarmServiceV1.h','AlarmServiceV2.h','AlarmClockEditV2.h']:shutil.copyfile(a.utilities/'lib/Alarm/include'/name,inc/name)
 includes=['-I'+str(x.resolve()) for x in [inc,root/'lib/NativeApps/include',a.utilities/'lib/Alarm/include',a.runtime/'sdk/driver',a.runtime/'sdk/app']]
 for san in (False,True):
  flags=['-std=c11','-O1','-g','-ffunction-sections','-fdata-sections',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if san else [])]
  service=['cc',*flags,*includes,*['-D'+x for x in ['ALARM_NATIVE_UTC','ALARM_NATIVE_SCHEDULES_ONLY','ALARM_SERVICE_TAGGED_V2','ALARM_VISUAL_ONLY','ALARM_DND_CONTROL','ALARM_SERVICE_CLOCK_EDIT_V2']],'-c',str(a.utilities/'Services/alarm_service/service.c'),'-o',str(out/'service.o')]
  commands.append(service);subprocess.run(service,check=True)
  cmd=['cc',*flags,'-Wl,--gc-sections',*includes,str(source),*[str(root/'lib/PortableApps/src'/n) for n in ['PortableSetTime.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']],str(out/'service.o'),'-o',str(out/'check')]
  commands.append(cmd);subprocess.run(cmd,check=True)
  for state in range(6):
   for direction in (range(4) if state<4 else [0]):
    r=subprocess.run([str(out/'check'),str(state),str(direction)],text=True,capture_output=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'});print(r.stdout,end='');print(r.stderr,end='');r.check_returncode();runs.append({'sanitize':san,'state':state,'direction':direction,'stdout':r.stdout})
files=[source,root/'lib/PortableApps/include/PortableClockCommit.h',root/'lib/PortableApps/src/settings_native_alarm.inc',root/'lib/PortableApps/src/settings_native_time.inc',pathlib.Path(__file__),*[x for x in (a.utilities/'Services/alarm_service').glob('*') if x.is_file()]]
(a.output/'evidence.json').write_text(json.dumps({'passed':True,'hardware_tested':False,'scope':'Actual Settings Save + actual service with modeled native clock/KV; no physical or whole-product claim','runs':runs,'commands':commands,'sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
