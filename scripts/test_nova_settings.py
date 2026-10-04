#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'build/nova-settings';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 for rotation in (0,180):
  common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION='+str(rotation),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
  if san:common+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  for name,count in [('nova_settings_test',2),('portable_alarm_settings_test',38),('portable_combined_settings_test',9),('portable_time_format_test',19)]:
   binary=out/f'{name}-{rotation}-{int(san)}';extra=['-DPORTABLE_SLEEP_SETTINGS'] if name in ('nova_settings_test','portable_alarm_settings_test') else [];subprocess.run([*common,*extra,str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps'/f'{name}.c'),'-o',str(binary)],check=True,timeout=120)
   for case in range(count):
    args=[str(binary),str(case)]
    if name=='nova_settings_test':
     frames=out/f'frames-{rotation}-{int(san)}-{case}';frames.mkdir(exist_ok=True);args.insert(1,str(frames))
    subprocess.run(args,check=True,timeout=20)
print('Nova Settings: 272 real-controller normal/sanitizer executions passed')
