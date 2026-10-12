#!/usr/bin/env python3
"""Actual selected X4 GT911 + ordered consumer + Files scroll release checks."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--product',type=Path,required=True)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
driver=a.product.resolve()/'minimal/drivers/x4pro_gt911/driver.c'
fixture=a.product.resolve()/'minimal/test/gt911_test.c'
manifest=driver.with_name('manifest.json')
assert json.loads(manifest.read_text())['version']=='0.1.10'
include=out/'sdk';shutil.copytree(a.sdk,include,dirs_exist_ok=True)
for name in ('PortableTouch.h','PortableTouchScroll.h','PortableNativeCustody.h'):
 shutil.copyfile(ROOT/'lib/PortableApps/include'/name,include/name)
source=ROOT/'test/native_apps/files_scroll_gt911_test.c'
records=[]
for mode,flags in [('normal',[]),('sanitized',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'])]:
 for rotation in (0,180):
  binary=out/f'{mode}-{rotation}'
  command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
   '-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_STAGE_LOGS',
   f'-DPORTABLE_TOUCH_ROTATION={rotation}','-DX4_GT911_FIXTURE='+json.dumps(str(fixture)),
   '-I'+str(include),str(source),str(driver),'-o',str(binary)]
  subprocess.run(command,check=True)
  for case in ('fling','held-release','reverse','bounds'):
   result=subprocess.run([str(binary),case],check=True,text=True,capture_output=True,
    timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
   print(result.stdout.strip(),flush=True);records.append(dict(mode=mode,rotation=rotation,case=case,result=result.stdout.strip()))
(out/'evidence.json').write_text(json.dumps(dict(hardware_tested=False,runs=records,
 source_sha256={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [driver,fixture,manifest,source,Path(__file__).resolve(),*[include/f for f in ('PortableTouch.h','PortableTouchScroll.h')]]}),indent=2)+'\n')
