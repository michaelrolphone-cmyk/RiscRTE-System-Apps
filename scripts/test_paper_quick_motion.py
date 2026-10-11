#!/usr/bin/env python3
"""Actual paper Clock/adapter motion; provider doubles, no hardware timing claim."""
import os,subprocess
from pathlib import Path
from PIL import Image,ImageChops
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/paper-quick-motion';OUT.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 for landscape in (False,True):
  flags=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_PAPER_TRANSITIONS']
  if os.environ.get('RASTER_SNAPSHOT'):flags+=['-DPORTABLE_RASTER_SNAPSHOT']
  if landscape:flags+=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90']
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  binary=OUT/f'motion-{sanitized}-{landscape}'
  sources=[ROOT/'Apps/paper_clock.c',ROOT/'test/native_apps/paper_quick_motion_test.c']+[ROOT/'lib/PortableApps/src'/p for p in ('adapter.c','quick_actions.c','quick_render.c','quick_session.c')]
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',binary],check=True)
  frames=OUT/f'frames-{sanitized}-{landscape}';frames.mkdir(exist_ok=True)
  subprocess.run([binary,frames],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=20)
  files=sorted(frames.glob('frame-*.pbm'),key=lambda p:int(p.stem.split('-')[1]))
  images=[Image.open(p).convert('RGB') for p in files]
  if landscape:images=[im.rotate(270,expand=True) for im in images]
  assert images[0].tobytes()==images[-1].tobytes()
  masks=[ImageChops.difference(images[0],im).getbbox() for im in images[1:-1]]
  assert sum(bool(b and b[3]-b[1]<750) for b in masks)>=2
  assert len({im.tobytes() for im in images})>=5
  for i,im in enumerate(images):im.save(frames/f'view-{i:02}.png')
  images[0].save(frames/'motion.gif',save_all=True,append_images=images[1:],duration=120,loop=0)
print('Paper motion: normal/ASan+UBSan, portrait/native, intermediate pixels and exact restore PASS')
