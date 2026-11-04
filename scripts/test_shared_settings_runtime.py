#!/usr/bin/env python3
"""Shared Settings Runtime with actual alarm service, modeled clock/storage ports."""
import argparse, pathlib, subprocess, tempfile, json, os, shutil, hashlib
p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--driver-sdk',type=pathlib.Path);p.add_argument('--utilities',type=pathlib.Path,required=True);p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
inc=['-I'+str(x) for x in [root/'lib/PortableApps/include',a.sdk,a.utilities/'lib/Alarm/include',a.runtime/'sdk/driver',a.runtime/'sdk/app',root/'lib/NativeApps/include']]
records=[]
with tempfile.TemporaryDirectory() as tmp:
 staged=pathlib.Path(tmp)/'include';staged.mkdir()
 for directory in [root/'lib/PortableApps/include',a.runtime/'sdk/driver',a.runtime/'sdk/app',a.utilities/'lib/Alarm/include',a.sdk,*([a.driver_sdk] if a.driver_sdk else [])]:
  for header in directory.glob('*.h'):shutil.copyfile(header,staged/header.name)
 sdk_hashes={h.name:hashlib.sha256(h.read_bytes()).hexdigest() for h in staged.glob('*.h')}
 inc=['-I'+str(staged),'-I'+str(root/'lib/NativeApps/include')]
 for profile in (root/'Services/settings_policy/profiles').glob('*.json'):
  generated=pathlib.Path(tmp)/profile.stem
  subprocess.run(['python3',str(root/'scripts/generate_shared_settings_policy.py'),str(profile),str(generated)],check=True)
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*inc,'-I'+str(generated),'-DALARM_SERVICE_TAGGED_V2','-DALARM_SERVICE_CLOCK_EDIT_V2','-DPORTABLE_PREFERENCE_ANY_STORE','-DPORTABLE_DESK_POINTS_FACE','-c',str(root/'Services/settings_policy/service.c'),'-o',str(generated/'policy.o')],check=True)
 for san in (False,True):
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if san else [])
  service=pathlib.Path(tmp)/'service.o'
  subprocess.run(['cc',*flags,*inc,*['-D'+s for s in ['ALARM_NATIVE_UTC','ALARM_NATIVE_SCHEDULES_ONLY','ALARM_SERVICE_TAGGED_V2','ALARM_VISUAL_ONLY','ALARM_DND_CONTROL','ALARM_SERVICE_CLOCK_EDIT_V2']],'-c',str(a.utilities/'Services/alarm_service/service.c'),'-o',str(service)],check=True)
  binary=pathlib.Path(tmp)/'test'
  sources=['PortableSettingsRuntime.c','PortableSettingsModel.c','PortableSetTime.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
  subprocess.run(['cc',*flags,*inc,'-DALARM_SERVICE_TAGGED_V2','-DALARM_SERVICE_CLOCK_EDIT_V2','-DALARM_NATIVE_UTC','-DPORTABLE_PREFERENCE_ANY_STORE','-DPORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL',str(root/'test/native_apps/shared_settings_runtime_service_test.c'),*[str(root/'lib/PortableApps/src'/s) for s in sources],str(service),'-Wl,--gc-sections','-o',str(binary)],check=True)
  for state in range(8):
   for direction in (range(10) if state==7 else range(4) if state<4 else [0]):
    r=subprocess.run([str(binary),str(state),str(direction)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1'},timeout=30)
    print(r.stdout,end='');print(r.stderr,end='');r.check_returncode();records.append({'sanitized':san,'state':state,'direction':direction,'result':r.stdout})
(a.output/'evidence.json').write_text(json.dumps({'passed':True,'hardware_tested':False,'scope':'Actual shared Settings Runtime and alarm service; clock, preference KV, availability and capability broker are modeled. Native and RTC modes, atomic partial outcomes, cancellation, stale authority and retention cases covered. Physical devices not exercised.','runs':records,'sdk_sha256':sdk_hashes,'source_sha256':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in [pathlib.Path(__file__),root/'lib/PortableApps/src/PortableSettingsRuntime.c',root/'lib/PortableApps/include/PortableSettingsRuntime.h',root/'lib/PortableApps/src/PortableSettingsModel.c',root/'lib/PortableApps/include/PortablePreferenceAtomic.h',root/'test/native_apps/shared_settings_runtime_service_test.c',*[path for path in (a.utilities/'Services/alarm_service').glob('*') if path.is_file()]]}},indent=2)+'\n')
