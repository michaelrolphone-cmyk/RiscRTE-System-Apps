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
profiles=[('settings-sleep',['-DPORTABLE_SLEEP_SETTINGS'],list(range(40,48))),('settings-return',['-DPORTABLE_RETURN_APP="springboard.elf"'],[0,1,25,29,34,35]),('settings',[],list(range(13))+list(range(14,20))+list(range(25,30))+[34,35]),
 ('settings-denver',['-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_TOUCH_ROTATION=180'],[0]+list(range(20,30))+[34,35]),
 ('settings-navigation',['-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_INPUT_NAVIGATION'],[30,31,32,36,37]),
 ('settings-local-navigation',['-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL'],[30,31,32,36,37,38])]
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
# Opaque persisted sleep policy is shared byte-for-byte with the Watch Clock.
binary=out/'sleep-policy'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
 '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
 '-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'test/native_apps/portable_sleep_policy_test.c'),
 '-o',str(binary)],check=True,timeout=30)
subprocess.run([str(binary)],check=True,timeout=10)

# Generic adapter sleep boundaries, including real nested Settings controllers.
# The fixture supplies a fake local sleep hook; Watch hardware policy is tested
# independently by its deployment repository.
binary=out/'idle-sleep'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
 '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
 str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_idle_sleep_test.c'),
 '-o',str(binary)],check=True,timeout=60)
for scenario in range(8):subprocess.run([str(binary),str(scenario)],check=True,timeout=10)

# Retained-error unwind must not reach normal provider I/O from nested Settings.
binary=out/'retained-sleep'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
 '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
 str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_retained_sleep_test.c'),
 '-o',str(binary)],check=True,timeout=60)
subprocess.run([str(binary)],check=True,timeout=10)
subprocess.run([str(binary),'time-format'],check=True,timeout=10)

# Shared namespace-1 time-format contract, real selector, and local civil labels.
for name,flags in [('time-format',[]),('time-format-denver',['-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_TOUCH_ROTATION=180']),
                   ('time-format-navigation',['-DPORTABLE_INPUT_NAVIGATION'])]:
 binary=out/name
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
  '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',*flags,
  '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
  str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_time_format_test.c'),'-o',str(binary)],check=True,timeout=60)
 for scenario in range(19):
  if scenario in (13,14) and flags!=['-DPORTABLE_INPUT_NAVIGATION']:continue
  subprocess.run([str(binary),str(scenario)],check=True,timeout=10)

# Historical catalogs remain 17; SDR opts into18, later HID-enabled Watch into20.
common=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
for mode,sanitizers in [('normal',[]),('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'])]:
 for profile,limit in [('historical',[]),('explicit-17',['-DPORTABLE_CATALOG_LIMIT=17']),('sdr',['-DPORTABLE_CATALOG_LIMIT=18']),('intermediate',['-DPORTABLE_CATALOG_LIMIT=19']),('hid-watch',['-DPORTABLE_CATALOG_LIMIT=20'])]:
  for count in (0,1,16,17,18,19,20,21,128,4294967295):
   binary=out/('catalog-'+mode+'-'+profile+'-'+str(count))
   subprocess.run([*common,*sanitizers,*limit,'-DTEST_CATALOG_COUNT='+str(count)+'U',
    str(ROOT/'test/native_apps/portable_catalog_capacity_test.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),'-o',str(binary)],check=True,timeout=60)
   subprocess.run([str(binary)],check=True,timeout=10)
# Reject unsupported compile-time limits before any app can be built.
for limit in (-1,0,16,21,128,4294967295):
 result=subprocess.run([*common,'-DPORTABLE_CATALOG_LIMIT='+str(limit),'-fsyntax-only',
  str(ROOT/'lib/PortableApps/src/adapter.c')],capture_output=True,text=True,timeout=60)
 assert result.returncode and 'PORTABLE_CATALOG_LIMIT must be between 17 and 20' in result.stderr, limit
