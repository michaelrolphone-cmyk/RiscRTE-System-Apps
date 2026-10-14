#!/usr/bin/env python3
"""Pixel-exact restored raster comparison; host costs are not device timings."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1];out=root/'build/springboard-raster-cost';out.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 binary=out/('sanitized' if sanitized else 'normal')
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitized else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'test/native_apps/springboard_raster_cost_test.c'),'-o',str(binary)],check=True)
 p=subprocess.run([str(binary)],capture_output=True,text=True,check=True,timeout=120)
 (out/(binary.name+'.txt')).write_text(p.stdout);print(p.stdout,end='')
