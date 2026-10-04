#!/usr/bin/env python3
"""Exercise actual optional audio/alarm/sleep adapter without physical I/O.\nCurrent-main forward-port regression for the Watch Audio Tools lifecycle.\n"""
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/portable-audio'
out.mkdir(parents=True,exist_ok=True)
for suffix,flags in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'])]:
    exe=out/suffix
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
        '-DPORTABLE_AUDIO_SESSION',*flags,
        '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
        str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_alarm_test.c'),
        '-o',str(exe)],check=True)
    for scenario in range(13):
        subprocess.run([str(exe),str(scenario)],check=True,timeout=10,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
