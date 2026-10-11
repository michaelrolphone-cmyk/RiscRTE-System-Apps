#!/usr/bin/env python3
"""Selected native API1/API2 terminal guards in the actual shared adapter."""
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True)
a=p.parse_args();out=ROOT/'build/alarm-terminal';out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as temp:
 include=Path(temp)/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include);shutil.copytree(ROOT/'lib/PortableApps/time',Path(temp)/'time')
 for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):shutil.copyfile(a.runtime/'sdk/app'/name,include/name)
 for api in (1,2):
  for san in (False,True):
   flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_STAGE_LOGS']
   if api==2:flags+=['-DALARM_SERVICE_TAGGED_V2']
   if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   flags+=['-I'+str(include),'-I'+str(a.utilities/'lib/Alarm/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
   if api==2:
    client=out/f'client-{api}-{int(san)}';subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'test/native_apps/portable_alarm_v2_test.c'),'-o',str(client)],check=True);subprocess.run([str(client)],check=True,env=env)
   exe=out/f'adapter-{api}-{int(san)}';sources=[ROOT/'test/native_apps/portable_alarm_v2_adapter_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]]
   subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,sources),'-Wl,--wrap=free','-o',str(exe)],check=True)
   cases=['storage','rtc','step-retained','status-retained','copied-retained','quick-retained','ack-retained','stop-retained','global-retained']
   if api==2:cases+=['visual','sound']
   for case in cases:subprocess.run([str(exe),case],check=True,env=env,timeout=20)
print('Selected API1/API2: 40 adapter cases plus descriptor/client matrices PASS')
