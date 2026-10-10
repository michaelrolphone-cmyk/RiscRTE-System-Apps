#!/usr/bin/env python3
"""Bounded app-owned RTC interpretation record; no device/runtime writes."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
 for sanitized in (False,True):
  binary=Path(directory)/('basis-san' if sanitized else 'basis')
  flags=['-std=c11','-Wall','-Wextra','-Werror','-pedantic']
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
  subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(root/'lib/PortableApps/include'),str(root/'test/native_apps/portable_rtc_basis_test.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
