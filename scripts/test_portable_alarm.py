#!/usr/bin/env python3
"""Exercise actual optional foreground adapter; no hardware claims from fakes."""
import os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/portable-alarm';out.mkdir(parents=True,exist_ok=True)
for suffix,flags in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'])]:
 exe=out/suffix
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,
  '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
  str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_alarm_test.c'),'-o',str(exe)],check=True)
 for scenario in range(11):subprocess.run([str(exe),str(scenario)],check=True,timeout=10)
