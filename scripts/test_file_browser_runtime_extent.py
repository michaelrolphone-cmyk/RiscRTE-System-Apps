#!/usr/bin/env python3
"""Real Files init/poll/cleanup against old, current and future native tables."""
import os
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RUNTIME=Path(os.environ['RUNTIME_REPO'])
OUT=ROOT/'build/file-browser-runtime-extent'
OUT.mkdir(parents=True,exist_ok=True)
for sanitize in (False,True):
    binary=OUT/('test-san' if sanitize else 'test')
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if sanitize else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
        '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(RUNTIME/'sdk/app'),
        str(ROOT/'test/native_apps/file_browser_runtime_extent_test.c'),'-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
    for mode in ('old','clock','current','future','null'):
        subprocess.run([str(binary),mode],check=True,env=env)
