#!/usr/bin/env python3
"""Exercise actual optional audio/alarm/sleep adapter without physical I/O.\nCurrent-main forward-port regression for the Watch Audio Tools lifecycle.\n"""
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/portable-audio'
out.mkdir(parents=True,exist_ok=True)
variants=[('plain',[]),('san',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']),('nova',['-DPORTABLE_NOVA_UI']),('nova-san',['-DPORTABLE_NOVA_UI','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'])]
variants += [(name+'-continuous',flags+['-DPORTABLE_AUDIO_CONTINUOUS_CAPTURE']) for name,flags in variants.copy()]
for suffix,flags in variants:
    exe=out/suffix
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
        '-DPORTABLE_AUDIO_SESSION',*flags,
        '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
        str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_alarm_test.c'),
        '-o',str(exe)],check=True)
    for scenario in range(19 if '-DPORTABLE_AUDIO_CONTINUOUS_CAPTURE' in flags else 16):
        subprocess.run([str(exe),str(scenario)],check=True,timeout=10)
