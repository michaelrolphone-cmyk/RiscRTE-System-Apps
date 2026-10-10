#!/usr/bin/env python3
"""Pure desk-clock policy; no display, RTC retention or hardware claim."""
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='desk-clock-policy-') as temp:
    for sanitized in (False,True):
        out=pathlib.Path(temp)/('sanitized' if sanitized else 'normal')
        flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-pedantic',*flags,'-I'+str(root/'lib/PortableApps/include'),str(root/'test/native_apps/portable_desk_clock_test.c'),'-o',str(out)],check=True)
        subprocess.run([str(out)],check=True,timeout=20)
