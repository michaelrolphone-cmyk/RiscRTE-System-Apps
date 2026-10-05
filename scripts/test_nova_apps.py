#!/usr/bin/env python3
"""Nova UI through real app/adapter source and deterministic fake hardware."""
import argparse,os,subprocess,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--utilities',type=Path,required=True);a=p.parse_args();u=a.utilities.resolve()
out=ROOT/'build/nova-apps';out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION=0','-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_ALARM_CLIENT']
for i,name in enumerate(['calculator','stopwatch','battery','alarms','countdown'],1):
 for sanitize in (False,True):
  binary=out/f'{name}-{int(sanitize)}'
  owner='CALCULATOR_RETURN_APP' if name=='calculator' else 'ALARM_RETURN_APP' if name in ('alarms','countdown') else 'PORTABLE_RETURN_APP'
  cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-D'+owner+'="springboard.elf"','-DNOVA_APP_ID='+str(i),'-DNOVA_APP_SOURCE='+json.dumps(str(u/'Apps'/f'{name}.c'))]
  if sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  cmd+=['-I'+str(d) for d in (ROOT/'lib/PortableApps/include',ROOT/'lib/NativeApps/include',u/'lib/Alarm/include')]
  cmd += [str(ROOT/'test/native_apps/nova_apps_test.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)]
  subprocess.run(cmd,check=True,timeout=120)
  for case in range(2):
   frames=out/f'{name}-{int(sanitize)}-case{case}';frames.mkdir(exist_ok=True)
   subprocess.run([str(binary),str(frames),str(case)],check=True,timeout=60)
print('20 production Nova touch/controller/cleanup executions passed')
