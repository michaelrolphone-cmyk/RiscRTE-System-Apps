#!/usr/bin/env python3
import argparse,os,subprocess,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--utilities',type=Path);a=p.parse_args()
for name,pin in json.loads((ROOT/'lib/PortableApps/SOURCES.json').read_text()).items():
 assert hashlib.sha256((ROOT/'lib/PortableApps/include'/name).read_bytes()).hexdigest()==pin['sha256'], name
out=ROOT/'build/portable';out.mkdir(parents=True,exist_ok=True)
for name,source,flags in [('springboard',ROOT/'Apps/springboard.c',[])]+([('battery',a.utilities.resolve()/'Apps/battery.c',['-DBATTERY_TEST'])] if a.utilities else []):
 binary=out/name
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(source),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/portable_adapter_test.c'),'-o',str(binary)],check=True,timeout=60)
 for n in range(8 if name == "battery" else 5):subprocess.run([str(binary),str(n)],check=True,timeout=10)

# The original Settings app executes through the production compact view.
profiles=[('settings',[],list(range(13))+list(range(14,20))+list(range(25,30))+[34,35]),
 ('settings-denver',['-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_TOUCH_ROTATION=180'],[0]+list(range(20,30))+[34,35]),
 ('settings-navigation',['-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_INPUT_NAVIGATION'],[30,31,32,36,37])]
for name,flags,cases in profiles:
 binary=out/name
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
  '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',*flags,
  '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
  str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_settings_test.c'),
  '-o',str(binary)],check=True,timeout=60)
 for n in cases:subprocess.run([str(binary),str(n)],check=True,timeout=10)
for name,flags in [('raw',[]),('denver',['-DPORTABLE_RTC_UTC8_DENVER'])]:
 binary=out/('time-'+name)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
  '-fsanitize=undefined',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),
  str(ROOT/'test/native_apps/portable_time_test.c'),'-o',str(binary)],check=True,timeout=60)
 subprocess.run([str(binary)],check=True,timeout=10)
