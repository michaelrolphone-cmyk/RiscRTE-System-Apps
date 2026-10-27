#!/usr/bin/env python3
"""Selected editor Settings controller: exact Save ordering and custody matrix."""
import argparse,pathlib,subprocess,tempfile,os,json,hashlib,shutil
from build_portable_settings import native_time_sdk,stage_native_time_sdk
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=pathlib.Path);p.add_argument('--utilities',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
class Options: native_time_runtime_repo=a.runtime
runs=[]
with tempfile.TemporaryDirectory() as tmp:
 out=pathlib.Path(tmp);inc=stage_native_time_sdk(out,native_time_sdk(Options,p));source=root/'test/native_apps/settings_native_alarm_test.c'
 for name in ['AlarmServiceV1.h','AlarmServiceV2.h','AlarmClockEditV2.h']:shutil.copyfile(a.utilities/'lib/Alarm/include'/name,inc/name)
 for san in (False,True):
  cmd=['cc','-std=c11','-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if san else []),'-I'+str(inc),'-I'+str(root/'lib/NativeApps/include'),str(source),*[str(root/'lib/PortableApps/src'/n) for n in ['PortableSetTime.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']],'-o',str(out/'check')]
  subprocess.run(cmd,check=True)
  for mode in range(12):
   r=subprocess.run([str(out/'check'),str(mode)],text=True,capture_output=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'});print(r.stdout,end='');print(r.stderr,end='');r.check_returncode();runs.append({'sanitize':san,'mode':mode,'stdout':r.stdout})
files=[source,root/'lib/PortableApps/include/PortableClockCommit.h',root/'lib/PortableApps/src/settings_native_alarm.inc',root/'lib/PortableApps/src/settings_native_time.inc',pathlib.Path(__file__)]
(a.output/'evidence.json').write_text(json.dumps({'passed':True,'hardware_tested':False,'scope':'Actual Settings seed/readback and tagged editor protocol with modeled service; service persistence covered separately','runs':runs,'sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
