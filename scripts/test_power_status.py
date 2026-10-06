#!/usr/bin/env python3
"""No-device test of the opt-in, app-local power read adapter."""
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/power-status';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 exe=out/('test-'+str(int(san)))
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/power_status_adapter_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
