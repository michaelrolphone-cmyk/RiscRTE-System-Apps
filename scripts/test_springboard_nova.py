#!/usr/bin/env python3
"""Compile the actual shared launcher/client and motion model under UBSan."""
import hashlib,json,os,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1];out=ROOT/'build/nova';out.mkdir(parents=True,exist_ok=True)
fonts=ROOT/'lib/PortableApps/fonts';pins=json.loads((fonts/'SOURCES.json').read_text())
for file in ['icons.inc','text.inc']:assert hashlib.sha256((fonts/file).read_bytes()).hexdigest()==pins[file+'_sha256']
assert 'solid:f2f2' in pins['icons'] and pins['glyph_names']['solid:f2f2']=='stopwatch'
flags=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all']
subprocess.run([*flags,str(ROOT/'test/native_apps/springboard_motion_test.c'),'-o',str(out/'motion')],check=True)
subprocess.run([str(out/'motion')],check=True)
for count in [0,1,2,3]:
 binary=out/('launcher-'+str(count))
 subprocess.run([*flags,'-DCATALOG_COUNT='+str(count),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/springboard.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/springboard_nova_test.c'),'-o',str(binary)],check=True)
 scenarios=list(range(35))+[36,37] if count==3 else [0,1,2,6,14,15,16]
 for scenario in scenarios:subprocess.run([str(binary),str(scenario)],check=True,timeout=10)

for name,extra,scenarios in [('rotation180',['-DPORTABLE_TOUCH_ROTATION=180','-DTEST_ROTATION_180'],[0,1,2,6,12,16]),('denver',['-DPORTABLE_RTC_UTC8_DENVER'],[0,32,33]),('handoff',['-DPORTABLE_RETAINED_RGB565_HANDOFF','-DPORTABLE_FORCE_FULL_FRAMES'],list(range(35))+[36,37])]:
 binary=out/name
 subprocess.run([*flags,*extra,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/springboard.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/springboard_nova_test.c'),'-o',str(binary)],check=True)
 for scenario in scenarios:subprocess.run([str(binary),str(scenario)],check=True,timeout=10)

for test in ['bounds','timing']:
 binary=out/('motion-'+test)
 subprocess.run([*flags,'-O1','-I'+str(ROOT/'Apps'),str(ROOT/('test/native_apps/springboard_motion_'+test+'_test.c')),'-lm','-o',str(binary)],check=True)
 result=subprocess.run([str(binary)],check=True,timeout=60,capture_output=True,text=True)
 (out/(test+'.txt')).write_text(result.stdout)
 print(result.stdout.splitlines()[-1])

subprocess.run([*flags,'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/springboard_tap_layers_test.c'),'-o',str(out/'tap-layers')],check=True)
subprocess.run([str(out/'tap-layers')],check=True)

# Direct production caption pixels on bright backgrounds, with ASan/UBSan.
subprocess.run([*flags,'-fsanitize=address','-fno-omit-frame-pointer','-no-pie',
 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
 str(ROOT/'test/native_apps/springboard_caption_test.c'),'-o',str(out/'caption-pixels')],check=True)
subprocess.run([str(out/'caption-pixels')],check=True)

binary=out/'daily-catalog'
subprocess.run([*flags,'-DNOVA_DAILY_CATALOG','-DCATALOG_COUNT=5','-DPORTABLE_RTC_UTC8_DENVER',
 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
 str(ROOT/'Apps/springboard.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),
 str(ROOT/'test/native_apps/springboard_nova_test.c'),'-o',str(binary)],check=True)
for scenario in [0,6,14,16,32,33]:subprocess.run([str(binary),str(scenario)],check=True,timeout=10)
