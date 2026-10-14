#!/usr/bin/env python3
"""Actual deployed RGB tap settings and calibration through explicit copied USER saves."""
import argparse,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
runs=[]
for rotation in (0,180):
 for native in (False,True):
  exe=a.output/f'test-{rotation}-{int(native)}'
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION='+str(rotation),'-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  if native:flags+=['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DSETTINGS_FIXTURE_WIDTH=410','-DSETTINGS_FIXTURE_HEIGHT=502']
  subprocess.run(['cc',*flags,*['-I'+str(root/q) for q in ('lib/PortableApps/include','lib/NativeApps/include')],str(root/'Apps/settings.c'),str(root/'test/native_apps/tap_atomic_test.c'),'-o',str(exe)],check=True)
  for case in range(20):
   for profile in (1,2):
    r=subprocess.run([str(exe),str(case),str(profile)],capture_output=True,text=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    if r.returncode:raise RuntimeError(f'{rotation=} {native=} {case=} {profile=}\n'+r.stdout+r.stderr)
    runs.append({'rotation':rotation,'native':native,'case':case,'sensor_profile':profile,'result':r.stdout.strip()})
  print(f'Actual tap atomic UI rotation={rotation} native={native}: 40 ASan/UBSan cases PASS',flush=True)
(a.output/'receipt.json').write_text(json.dumps({'hardware_tested':False,'runs':runs},indent=2)+'\n')
