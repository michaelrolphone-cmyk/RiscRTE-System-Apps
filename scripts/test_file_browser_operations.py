#!/usr/bin/env python3
"""Exercise full paper file-management controller with transactional fake volumes."""
import os,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];out=r/'build/file-browser-operations';out.mkdir(parents=True,exist_ok=True);frames=out/'frames';frames.mkdir(exist_ok=True)
for sanitize in (False,True):
    binary=out/('operations-asan' if sanitize else 'operations')
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror',*flags,'-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/portable_file_browser_operations_test.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=30,env={**os.environ,'FILE_BROWSER_PAPER_FRAMES':str(frames)})
