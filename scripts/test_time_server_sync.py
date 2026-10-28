#!/usr/bin/env python3
"""Read-only sampler and real shared alarm-aware UTC commit; no sockets/devices."""
import argparse,hashlib,json,os,pathlib,shutil,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--utilities',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for folder in ('driver','hardware','app'):
 for f in (a.runtime/'sdk'/folder).glob('*.h'):shutil.copyfile(f,inc/f.name)
for n in ('AlarmServiceV1.h','AlarmServiceV2.h','AlarmClockEditV2.h'):shutil.copyfile(a.utilities/'lib/Alarm/include'/n,inc/n)
assert (inc/'RiscTimeSampleV1.h').read_bytes()==(ROOT/'lib/PortableApps/include/RiscTimeSampleV1.h').read_bytes()
base=(ROOT/'test/native_apps/portable_wifi_test.c').read_text()
base=base.replace('#include "../../lib/PortableApps/src/adapter.c"','#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c')))
base=base.replace('#include "../../Apps/wifi_settings.c"','#define PORTABLE_WIFI_TIME_SYNC\n#include '+json.dumps(str(ROOT/'Apps/wifi_settings.c')))
(out/'controller-base.c').write_text(base)
workflow=(ROOT/'test/native_apps/wifi_workflow_test.c').read_text().replace('#include "portable_wifi_test.c"','#include '+json.dumps(str(out/'controller-base.c'))).replace('int main(int argc,char**argv)','int workflow_fixture_main(int argc,char**argv)')
(out/'controller-workflow.c').write_text(workflow)
controller_inc=out/'controller-include';shutil.copytree(inc,controller_inc,dirs_exist_ok=True)
shutil.copyfile(ROOT/'lib/PortableApps/include/AlarmServiceV1.h',controller_inc/'AlarmServiceV1.h')
runs=[]
for sanitized in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if sanitized else []
 for name in ('time_sync','wifi_time_sync_policy','wifi_time_commit','wifi_time_sync_controller'):
  sources=[ROOT/'test/native_apps'/f'{name}_test.c']
  if name!='wifi_time_sync_policy':sources.append(ROOT/'lib/PortableApps/src/PortableTimeSync.c')
  if name=='wifi_time_commit':sources += [ROOT/'lib/PortableApps/src'/n for n in ('PortableSetTime.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')]
  exe=out/(name+('-san' if sanitized else ''))
  cmd=['cc','-std=c11','-O1','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,*(['-I'+str(controller_inc)] if name=='wifi_time_sync_controller' else []),'-DWIFI_SYNC_FIXTURE='+json.dumps(str(out/'controller-workflow.c')),'-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',str(exe)]
  subprocess.run(cmd,check=True)
  for mode in (range(18) if name=='wifi_time_commit' else range(4) if name=='wifi_time_sync_controller' else (None,)):
   r=subprocess.run([str(exe),*([str(mode)] if mode is not None else [])],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'});print(r.stdout+r.stderr,end='',flush=True);r.check_returncode();runs.append({'name':name,'sanitized':sanitized,'mode':mode,'command':cmd,'stdout':r.stdout})
(out/'evidence.json').write_text(json.dumps({'passed':True,'hardware':False,'runs':runs,'source_sha256':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [ROOT/'lib/PortableApps/include/PortableClockCommit.h',ROOT/'lib/PortableApps/src/PortableTimeSync.c',ROOT/'lib/PortableApps/src/wifi_time_commit.inc',ROOT/'Apps/wifi_time_sync.inc']}},indent=2)+'\n')
