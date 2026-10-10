#!/usr/bin/env python3
"""Preserve the accepted Watch client's default and storage-reconciliation behavior."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/broadcast-client';out.mkdir(parents=True,exist_ok=True)
for sanitized in [False,True]:
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 binary=out/str(sanitized);subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,'-I'+str(root/'lib/PortableApps/include'),root/'test/native_apps/broadcast_client_test.c','-o',binary],check=True);subprocess.run([binary],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
