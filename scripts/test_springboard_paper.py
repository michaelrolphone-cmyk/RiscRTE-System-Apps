#!/usr/bin/env python3
"""Real shared Springboard/adapter MONO1 regression and optional PBM evidence."""
import hashlib,json,os,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1];out=ROOT/'build/paper';out.mkdir(parents=True,exist_ok=True)
pins=json.loads((ROOT/'lib/PortableApps/paper_fonts/SOURCES.json').read_text())
assert hashlib.sha256((ROOT/'lib/PortableApps/paper_fonts/text.inc').read_bytes()).hexdigest()==pins['text.inc_sha256']
for p,digest in pins['inputs_sha256'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest
flags=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
for count in [0,1,5,17]:
    binary=out/f'paper-{count}'
    subprocess.run([*flags,f'-DCATALOG_COUNT={count}','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/springboard.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/springboard_paper_test.c'),'-o',str(binary)],check=True)
    for scenario in [0,1,2,3,4,5,6,7,9]:subprocess.run([str(binary),str(scenario)],check=True,timeout=10)

# Native 800x480/100-byte-stride X4-like provider with portrait 480x800 touch.
binary=out/'paper-native-landscape'
subprocess.run([*flags,'-DCATALOG_COUNT=5','-DPORTABLE_DISPLAY_ROTATION=90','-DTEST_NATIVE_LANDSCAPE','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/springboard.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/springboard_paper_test.c'),'-o',str(binary)],check=True)
for scenario in [0,1,2,3,4,5,6,7,9]:subprocess.run([str(binary),str(scenario)],check=True,timeout=10)
for name,program in [('portrait',out/'paper-5'),('native',binary)]:
    env={**os.environ,'PAPER_FRAME':str(out/(name+'.pbm'))}
    subprocess.run([str(program),'0'],env=env,check=True,timeout=10)
portrait=(out/'portrait.pbm').read_bytes().split(b'\n',2)[2]
native=(out/'native.pbm').read_bytes().split(b'\n',2)[2]
for y in range(800):
    for x in range(480):
        assert bool(portrait[y*60+x//8]&(0x80>>(x%8)))==bool(native[(479-x)*100+y//8]&(0x80>>(y%8)))
print('Native 800x480 raster is pixel-exact to logical portrait; touch launches unchanged')

binary=out/'paper-alarm'
subprocess.run([*flags,'-DCATALOG_COUNT=5','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_DISPLAY_ROTATION=90','-DTEST_NATIVE_LANDSCAPE','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/springboard.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/springboard_paper_test.c'),'-o',str(binary)],check=True)
for scenario in [0,1,2,3,4,5,6,7,9,10,12]:subprocess.run([str(binary),str(scenario)],check=True,timeout=10)
