#!/usr/bin/env python3
"""Qualify actual common Home sleep source with controlled capability tables."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','utilities','watch','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();inc=out/'include';inc.mkdir(parents=True,exist_ok=True)
shutil.copytree(root/'lib/PortableApps/include',inc,dirs_exist_ok=True)
for group in ('app','driver','hardware'):
 for source in (a.runtime/'sdk'/group).glob('*.h'):shutil.copyfile(source,inc/source.name)
for source in (a.utilities/'lib/Alarm/include').glob('*.h'):shutil.copyfile(source,inc/source.name)
for name in ('RiscDisplayPowerV1.h','RiscPowerSleepV1.h','RiscTimedSleepV1.h','RiscLightSleepV1.h','RiscDeepSleepV1.h'):
 shutil.copyfile(a.watch/'sdk/driver'/name,inc/name)
shutil.copyfile(a.watch/'apps/runtime_features/sdk/RiscRetainedWakeV1.h',inc/'RiscRetainedWakeV1.h')
shutil.copytree(root/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
cases=('normal','cancelled','stage-refused','stage-retained','panel-refused','power-refused','alarm-retained','alarm-unknown','alarm-context','boot-absent','boot-context','boot-dirty','panel-context','power-context','hold-context','entry-context','light-context','light-radio-context','input-context','input-restore-context','clear-context','stage-context','input-refused','input-retained','hold-refused','hold-retained','entry-retained','entry-zero','expired','rewound','record-changed','power-restore','panel-restore','input-restore','clear-retained','wake-retained','light','light-refused','light-retained','ui-refused','absent','null-table','release-failed','wake-clear-missing','alarm-no-resume','light-wifi-only','light-ble-only','light-radio-retained','light-radio-release-failed')
runs=[]
for mode in ('normal','sanitized'):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
 binary=out/mode
 defines=('PORTABLE_DESK_POINTS_FACE','PORTABLE_DESK_POINTS_SNAPSHOT','PORTABLE_ALARM_CLIENT','ALARM_SERVICE_TAGGED_V2','PORTABLE_X4_IDLE_POLICY','PORTABLE_DESK_CLOCK_SPARSE_START','PORTABLE_DESK_LOCK_HOME','PORTABLE_SHARED_HOME_CANVAS','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_ALARM_TERMINAL_RETENTION')
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,*['-D'+d for d in defines],'-I'+str(inc),'-I'+str(root/'sdk/app'),str(root/'test/native_apps/home_sleep_test.c'),'-o',str(binary)]
 subprocess.run(command,check=True)
 for case in cases:
  result=subprocess.run([str(binary),case],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  (out/(mode+'-'+case+'.log')).write_text(result.stdout+result.stderr)
  if result.returncode:print(result.stdout+result.stderr);result.check_returncode()
  print(result.stdout,end='');runs.append({'mode':mode,'case':case,'output':result.stdout,'command':command})
files=[root/'lib/PortableApps/src/PortableHomeSleep.c',root/'lib/PortableApps/src/PortableHomeIdle.c',root/'test/native_apps/home_sleep_test.c',Path(__file__),*inc.glob('*.h')]
(out/'receipt.json').write_text(json.dumps({'runs':runs,'hardware_tested':False,'sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
