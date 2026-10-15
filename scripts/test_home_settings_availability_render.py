#!/usr/bin/env python3
"""Actual Home status raster and cache qualification, no provider/device I/O."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=json.loads((a.target/'build-evidence.json').read_text());inc=a.output/'include';shutil.copytree(r['paper_transition']['compiled_include_directory'],inc,dirs_exist_ok=True)
for h in (ROOT/'lib/PortableApps/include').glob('*.h'):
 if not h.name.startswith(('Risc','AlarmService')):shutil.copyfile(h,inc/h.name)
shutil.copytree(ROOT/'lib/PortableApps/time',a.output/'time',dirs_exist_ok=True)
flags=['-D'+x for x in 'PORTABLE_DESK_CLOCK PORTABLE_ALARM_CLIENT PORTABLE_APP_SLEEP_LOCAL PORTABLE_DESK_CLOCK_SPARSE_START PORTABLE_DESK_LOCK_HOME PORTABLE_DESK_POINTS_FACE PORTABLE_DESK_POINTS_SNAPSHOT PORTABLE_HOME_POINTS_NATIVE_UTC ALARM_NATIVE_UTC'.split()]
for sanitized in (False,True):
 out=a.output/('asan-ubsan' if sanitized else 'normal');out.mkdir(exist_ok=True)
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 exe=out/'render';subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror',*extra,*flags,'-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),'-ffunction-sections','-fdata-sections','-Wl,--gc-sections',str(ROOT/'test/native_apps/home_settings_availability_render_test.c'),str(ROOT/'lib/PortableApps/src/PortableTimeZone.c'),str(ROOT/'lib/PortableApps/src/PortableTimeZoneCatalog.c'),str(ROOT/'lib/PortableApps/src/PortableTimeZonePreference.c'),str(ROOT/'lib/PortableApps/src/PortableRealtimeClient.c'),'-o',str(exe)],check=True)
 subprocess.run([exe,out],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
 for p in out.glob('settings-*.pbm'):Image.open(p).save(p.with_suffix('.png'))
for p in (a.output/'normal').glob('settings-*.pbm'):assert p.read_bytes()==(a.output/'asan-ubsan'/p.name).read_bytes()
