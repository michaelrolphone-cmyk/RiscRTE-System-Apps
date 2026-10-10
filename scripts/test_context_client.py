#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/contexts'
out.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(root/'lib/PortableApps/include')]
    if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    binary=out/f'client-{int(sanitized)}'
    subprocess.run([os.environ.get('CC','cc'),*flags,str(root/'test/native_apps/context_client_test.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
