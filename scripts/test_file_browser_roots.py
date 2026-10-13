#!/usr/bin/env python3
"""Exercise the real three-root controller with existing fault-injecting volumes."""
import os, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
out=pathlib.Path(os.environ.get('OUTPUT_DIR',tempfile.mkdtemp(prefix='files-roots-')));out.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 flags=['-fsanitize=undefined','-fno-sanitize-recover=all'] if sanitized else []
 exe=out/('roots-ubsan' if sanitized else 'roots')
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'test/native_apps/file_browser_roots_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=30)
