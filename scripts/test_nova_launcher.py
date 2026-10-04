#!/usr/bin/env python3
import json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'build/nova-launcher';out.mkdir(parents=True,exist_ok=True)
apps=json.loads((ROOT/'tests/fixtures/nova-delivered-catalog.json').read_text())
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={'+','.join('{'+','.join('.'+k+'='+json.dumps(v) for k,v in a.items())+',.compatible=true}' for a in apps)+'};\nconst unsigned portable_catalog_count='+str(len(apps))+';\n')
for san in (False,True):
 flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_RETAINED_RGB565_HANDOFF','-DPORTABLE_HANDOFF_EAGER_MS=60','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_RETURN_APP="clock.elf"']
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 binary=out/f'launcher-{int(san)}';frames=out/f'frames-{int(san)}';frames.mkdir(exist_ok=True)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(ROOT/p) for p in ('lib/PortableApps/include','lib/NativeApps/include')],str(ROOT/'test/native_apps/nova_launcher_test.c'),str(ROOT/'Apps/springboard.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)],check=True)
 subprocess.run([str(binary),str(frames)],check=True,timeout=60)
