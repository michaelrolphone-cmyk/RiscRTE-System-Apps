#!/usr/bin/env python3
"""Byte-compare frozen flag-off adapter objects and compile the selected target."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--xtensa-cc',type=Path,required=True)
a=p.parse_args();out=ROOT/'build/alarm-retention-target';out.mkdir(parents=True,exist_ok=True)
profiles=[['PORTABLE_ALARM_CLIENT'],['PORTABLE_ALARM_CLIENT','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_INPUT_NAVIGATION','PORTABLE_QUICK_ACTIONS','PORTABLE_BLE_BROADCAST','PORTABLE_CONTEXTS_CLIENT','PORTABLE_QUICK_RADIOS','PORTABLE_LOW_BATTERY']]
profiles+=[profiles[1]+['PORTABLE_SETTINGS_APP','PORTABLE_SLEEP_SETTINGS'],profiles[1]+['PORTABLE_AUDIO_SESSION']]
flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror']
receipt={}
for i,profile in enumerate(profiles):
 paths=[]
 for label,root in [('before',a.baseline),('after',ROOT)]:
  obj=out/f'{i}-{label}.o';paths.append(obj)
  command=[a.xtensa_cc,*flags,*['-D'+d for d in profile],*['-I'+str(root/d) for d in ('lib/PortableApps/include','lib/NativeApps/include')],root/'lib/PortableApps/src/adapter.c','-c','-o',obj]
  subprocess.run(list(map(str,command)),check=True)
 assert paths[0].read_bytes()==paths[1].read_bytes(),f'Flag-off profile {i} changed'
 receipt[str(i)]={'flags':profile,'sha256':hashlib.sha256(paths[1].read_bytes()).hexdigest()}
 command=[a.xtensa_cc,*flags,'-DPORTABLE_ALARM_TERMINAL_RETENTION',*['-D'+d for d in profile],*['-I'+str(ROOT/d) for d in ('lib/PortableApps/include','lib/NativeApps/include')],ROOT/'lib/PortableApps/src/adapter.c','-c','-o',out/f'{i}-selected.o']
 subprocess.run(list(map(str,command)),check=True)
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Four Xtensa flag-off adapter objects byte-identical; four selected target profiles compile PASS')
