#!/usr/bin/env python3
"""Shared RGB Settings controller: explicit user saves and failure/reentry checks."""
import argparse,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
for sanitize in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
 binary=a.output/('sanitized' if sanitize else 'normal')
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O0','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,*['-I'+str(x) for x in (root/'lib/PortableApps/include',root/'lib/NativeApps/include',a.runtime.resolve()/'sdk/app')],str(root/'Apps/settings.c'),str(root/'test/native_apps/settings_atomic_rgb_test.c'),'-o',str(binary)],check=True)
 for case in range(6):subprocess.run([str(binary),str(case)],check=True,timeout=20)
