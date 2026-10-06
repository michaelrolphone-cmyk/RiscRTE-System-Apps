#!/usr/bin/env python3
"""Real Settings controller, persistent policy, and calibration regressions."""
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/tap-settings';out.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 for rotation in (0,180):
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION='+str(rotation)]
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  for test in ('tap_policy','tap_settings','tap_alarm_interruption','tap_quick_interruption'):
   target=out/f'{test}-{rotation}-{int(sanitized)}'
   sources=[ROOT/'test/native_apps'/f'{test}_test.c']
   if test!='tap_policy':sources.insert(0,ROOT/'Apps/settings.c')
   if test=='tap_quick_interruption':sources += [ROOT/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_render.c','quick_session.c')]
   subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',str(target)],check=True)
   if test!='tap_settings':subprocess.run([str(target)],check=True,timeout=20)
   else:
    for case in range(20):
     for profile in (1,2):subprocess.run([str(target),str(case),str(profile)],check=True,timeout=20)
print('Tap settings: 160 production UI cases plus policy/state-machine normal/ASan/UBSan suites passed; physical qualification pending')
