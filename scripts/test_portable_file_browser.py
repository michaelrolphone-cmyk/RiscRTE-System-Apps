#!/usr/bin/env python3
"""Actual portable File Browser controller and NOVA raster; fake providers only."""
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/file-browser';OUT.mkdir(parents=True,exist_ok=True)
for san,rotation,full in ((False,0,False),(True,0,False),(False,180,False),(True,180,False),(False,0,True),(True,0,True),(False,180,True),(True,180,True)):
    output=OUT/(('test-san' if san else 'test')+'-'+str(rotation)+'-'+str(int(full)))
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
    if full:flags.append('-DFILE_BROWSER_FULL_PROFILE')
    sources=[str(ROOT/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')] if full else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
      f'-DPORTABLE_TOUCH_ROTATION={rotation}','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
      str(ROOT/'test/native_apps/portable_file_browser_test.c'),*sources,'-o',str(output)],check=True)
    args=[str(output)]
    if not san and rotation==0 and not full:
        frames=OUT/'frames';frames.mkdir(exist_ok=True);args.append(str(frames))
    subprocess.run(args,check=True)
