#!/usr/bin/env python3
"""Checked scene custody and exact-revision visibility, normal and sanitized."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='component-scene-client-') as tmp:
    for sanitized in (False,True):
        output=Path(tmp)/('sanitized' if sanitized else 'normal')
        command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'sdk/app'),
                 str(ROOT/'test/native_apps/component_scene_client_test.c'),'-o',str(output)]
        if sanitized:
            command[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
        subprocess.run(command,check=True)
        subprocess.run([str(output)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1'})
