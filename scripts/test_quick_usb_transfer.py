#!/usr/bin/env python3
"""Production Springboard paper Quick Actions: launch-only USB ownership."""
import os,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/quick-usb-transfer';OUT.mkdir(parents=True,exist_ok=True)
for selected in (False,True):
 for san in (False,True):
  flags=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_DISPLAY_ROTATION=90','-DTEST_NATIVE_LANDSCAPE','-DTEST_SPRINGBOARD']
  if selected:flags+=['-DPORTABLE_QUICK_USB_TRANSFER']
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  target=OUT/f'quick-{int(selected)}-{int(san)}'
  sources=[ROOT/'Apps/springboard.c',ROOT/'test/native_apps/quick_usb_transfer_test.c']+[ROOT/'lib/PortableApps/src'/s for s in ('adapter.c','quick_actions.c','quick_render.c','quick_session.c')]
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',str(target)],check=True)
  for case in range(2):
   frames=OUT/f'frames-{int(selected)}-{int(san)}-{case}';frames.mkdir(exist_ok=True)
   subprocess.run([target,str(case),frames],check=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
   if selected and not san:
    for f in frames.glob('frame-*.pbm'):Image.open(f).rotate(270,expand=True).save(f.with_suffix('.png'))
print('8 selected/unselected normal/sanitized real Quick Actions launch/drag cases passed')
