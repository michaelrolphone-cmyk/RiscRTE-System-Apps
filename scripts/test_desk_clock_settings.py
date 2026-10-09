#!/usr/bin/env python3
"""Exercise the actual paper Settings controller and opaque preference records."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--evidence',action='store_true');args=parser.parse_args()
OUT=ROOT/'build/desk-settings';OUT.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
    exe=OUT/('settings-san' if sanitized else 'settings')
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,
        '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
        str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_desk_clock_settings_test.c'),'-o',str(exe)],check=True)
    for scenario in range(107):subprocess.run([str(exe),str(scenario)],check=True,timeout=10)
print('Desk Settings: 107 scenarios in plain and ASan/UBSan passed')

for sanitized in (False,True):
    exe=OUT/('retained-san' if sanitized else 'retained')
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,
        '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
        str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_desk_clock_settings_retained_test.c'),'-o',str(exe)],check=True)
    for scenario in range(4):subprocess.run([str(exe),str(scenario)],check=True,timeout=10)

for definition in ('-DPORTABLE_DISPLAY_ROTATION=0','-DTEST_DESK_RGB'):
    for sanitized in (False,True):
        exe=OUT/'unsupported'
        flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,
            definition,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
            str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_desk_clock_settings_test.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe),'0'],check=True,timeout=10)
for sanitized in (False,True):
    exe=OUT/'short-paper'
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,
        '-DTEST_DESK_SHORT','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
        str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_desk_clock_settings_test.c'),'-o',str(exe)],check=True)
    for scenario in [0,*range(47,55),59,80,81,82,97]:subprocess.run([str(exe),str(scenario)],check=True,timeout=10)
print('Desk Settings: 254 controller, persistence, retained and display-profile executions passed')

if args.evidence:
    from PIL import Image, ImageDraw
    captures=OUT/'captures';captures.mkdir(exist_ok=True)
    evidence=ROOT/'docs/desk-clock/reader-settings-evidence';evidence.mkdir(parents=True,exist_ok=True)
    for scenario in [19,20,21,22,*range(32,40),55,81,99,100]:
        subprocess.run([str(OUT/'settings'),str(scenario)],env=dict(os.environ,DESK_SETTINGS_FRAMES=str(captures)),check=True,timeout=10)
    frames=[];record={}
    for path in sorted(captures.glob('*.pbm')):
        native=Image.open(path);native.load();assert native.size==(800,480)
        logical=native.transpose(Image.Transpose.ROTATE_270)
        destination=evidence/(path.stem+'.png');logical.save(destination,optimize=True)
        assert Image.open(destination).tobytes()==logical.tobytes()
        record[destination.name]={'width':480,'height':800,'sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),
            'pixel_sha256':hashlib.sha256(logical.tobytes()).hexdigest()}
        frames.append((path.stem,logical))
    sheet=Image.new('RGB',(1920,((len(frames)+3)//4)*840),'#cccccc');draw=ImageDraw.Draw(sheet)
    for i,(name,frame) in enumerate(frames):
        x=(i%4)*480;y=(i//4)*840;draw.text((x+12,y+8),name,fill='black');sheet.paste(frame,(x,y+30))
    sheet.save(evidence/'contact-sheet.png',optimize=True)
    (evidence/'raster-sha256.json').write_text(json.dumps(record,indent=2)+'\n')
    print(len(frames),'native lossless paper captures verified')
