#!/usr/bin/env python3
"""Exercise production Wi-Fi footer controls at square and tall Watch sizes."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
root=Path(__file__).resolve().parents[1]
flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
rows=[]
for native in (False,True):
 for width,height in (((240,240),(410,502),(480,480)) if native else ((240,240),)):
  label=f'{"native" if native else "legacy"}-{width}x{height}'
  binary=a.output/label
  cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,
       *([] if native else ['-DTEST_LEGACY_WATCH_LAYOUT']),f'-DTEST_WIFI_RGB_WIDTH={width}',f'-DTEST_WIFI_RGB_HEIGHT={height}',
       '-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),
       str(root/'test/native_apps/wifi_watch_touch_test.c'),'-o',str(binary)]
  subprocess.run(cmd,check=True,timeout=90)
  for case in range(4):
   result=subprocess.run([binary,str(case)],capture_output=True,text=True,timeout=30,
     env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
   (a.output/f'{label}-{case}.log').write_text(result.stdout+result.stderr)
   print(result.stdout,end='',flush=True);result.check_returncode()
   rows.append({'profile':label,'case':case,'passed':True})
paths=['Apps/wifi_settings_portable.inc','lib/PortableApps/src/adapter.c','lib/PortableApps/src/wifi_nova.inc','lib/PortableApps/include/WatchViewLayout.h','test/native_apps/portable_wifi_test.c','test/native_apps/wifi_watch_touch_test.c']
(a.output/'receipt.json').write_text(json.dumps({'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'sanitized':a.sanitize,'results':rows,'sha256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'scope':'Production controller, renderer and touch adapter; synthetic radio, display, storage and input providers. Hardware untested.'},indent=2)+'\n')
