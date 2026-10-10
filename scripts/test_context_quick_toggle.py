#!/usr/bin/env python3
"""Saved master toggle, gesture cancellation, and temporary foreground training."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='context-toggle-') as tmp:
 for san in (False,True):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  for case,sources in {
   'gesture':['lib/PortableApps/src/quick_actions.c','lib/PortableApps/src/quick_render.c','test/native_apps/quick_core_test.c'],
   'storage':['lib/PortableApps/src/quick_actions.c','lib/PortableApps/src/quick_session.c','test/native_apps/quick_session_test.c'],
   'policy':['test/native_apps/context_toggle_test.c']}.items():
   target=Path(tmp)/(case+str(san))
   subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),*[root/s for s in sources],'-o',target],check=True)
   subprocess.run([target],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=60)
