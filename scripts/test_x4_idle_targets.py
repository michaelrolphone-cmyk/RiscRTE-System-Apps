#!/usr/bin/env python3
"""Build five explicit X4 idle development apps; never compose a product image."""
import argparse,os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('x4','runtime','power-sdk','display-sdk','utilities'):
 p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--cc',required=True);a=p.parse_args()
base=r.parent
common=['--alarm-client','--quick-actions','--quick-radios','--paper-transitions','--tagged-alarm-utilities',str(a.utilities),'--x4-idle-source',str(a.x4/'minimal/apps/portable_idle_sleep.c'),'--x4-idle-sdk',str(a.power_sdk),'--x4-idle-runtime-sdk',str(a.runtime/'sdk/driver')]
native=['--native-time-runtime-repo',str(a.runtime)]
clock=['--navigation','--local-sleep-source',str(a.x4/'minimal/apps/portable_sleep.c'),'--sleep-capability','x4.power','--sleep-sdk',str(a.power_sdk),'--desk-clock','--sparse-start','--retained-wake-sdk',str(a.runtime/'sdk/app'),'--desk-lock-home','--ble-broadcast','--paper-crossfade','--paper-display-sdk',str(a.display_sdk)]
jobs=[('clock','build_paper_clock.py',clock),('settings','build_portable_settings.py',['--settings-profile','x4-native-time',*native,'--sleep-settings','--alarm-settings']),('springboard','build_portable_springboard.py',['--time-profile','x4-native-time',*native]),('files','build_portable_file_browser.py',['--time-profile','x4-native-time',*native,'--storage-capability','storage.volume']),('wifi','build_portable_wifi.py',['--time-profile','x4-native-time',*native])]
env=dict(os.environ,NATIVE_APP_CC=a.cc,PLATFORMIO_SETTING_ENABLE_TELEMETRY='no')
failures=[]
(r/'build/idle-policy').mkdir(parents=True,exist_ok=True)
for label,builder,args in jobs:
 out=r/'build/idle-policy'/label;log=out.with_suffix('.log')
 with log.open('w') as f:
  result=subprocess.run([os.sys.executable,str(r/'scripts'/builder),*common,*args,'--output-dir',str(out)],cwd=r,env=env,stdout=f,stderr=f)
 print(label,result.returncode,flush=True)
 if result.returncode:failures.append(label);print(log.read_text()[-8000:],flush=True)
if failures:raise SystemExit('Failed targets: '+', '.join(failures))
