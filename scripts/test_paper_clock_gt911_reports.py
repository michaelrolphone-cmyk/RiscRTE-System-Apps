#!/usr/bin/env python3
"""Bind a simultaneous-contact Clock regression to selected GT911 ready reports.

The default records the known failure as a failure, alongside passing controls.
--require-fixed makes that failure fatal. All production sources are read-only.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--product',type=Path,required=True)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--require-fixed',action='store_true')
p.add_argument('--driver-version',default='0.1.9')
p.add_argument('--driver-sha256',default='da287cbd675b6a59131b97278a48c6cf1734b90e30a9e2674e72b0063c9a86d1')
a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
driver=a.product.resolve()/'minimal/drivers/x4pro_gt911/driver.c'
physical=a.product.resolve()/'minimal/test/gt911_test.c'
manifest=driver.with_name('manifest.json')
assert json.loads(manifest.read_text())['version']==a.driver_version
assert hashlib.sha256(driver.read_bytes()).hexdigest()==a.driver_sha256
inc=out/'include';inc.mkdir(exist_ok=True)
for header in a.sdk.resolve().glob('*.h'):
    shutil.copyfile(header,inc/header.name)
# Keep this legacy Clock profile's app headers (including its battery ABI).
# Stage missing provider headers from the canonical SDK; both sides see one
# common copy of every raw-touch/provider header, with all digests recorded.
for header in (ROOT/'lib/PortableApps/include').glob('*.h'):
    shutil.copyfile(header,inc/header.name)
shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
sources=[ROOT/'Apps/paper_clock.c',ROOT/'lib/PortableApps/src/adapter.c',
         ROOT/'test/native_apps/paper_clock_gt911_report_test.c',
         ROOT/'test/native_apps/paper_clock_gt911_backend.c',driver]
flags=['-DTEST_CLOCK_ORDERED_INPUT','-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90',
       '-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT',
       '-DX4_GT911_FIXTURE='+json.dumps(str(physical))]
runs=[]
for san in (False,True):
    exe=out/('clock-'+str(int(san)))
    sanitizers=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function',*flags,*sanitizers,'-I'+str(inc),
                    '-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',str(exe)],check=True)
    for mode in ('fast','slow','busy'):
        for name,scene,repeat in [('single-report-cancel',15,False),('rapid-swipe',14,False),
                                  ('rapid-tap',16,False),('later-unchanged-report',15,True),
                                  ('independent-earlier-swipe',18,True),('launch-refusal-retry',9,False)]:
            result=subprocess.run([str(exe),str(scene),mode,*(['repeat'] if repeat else [])],
                                  capture_output=True,text=True,timeout=15,
                                  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
            log=out/(name+'-'+mode+'-'+str(int(san))+'.log');log.write_text(result.stdout+result.stderr)
            if name not in ('later-unchanged-report','single-report-cancel'):assert result.returncode==0,(name,mode,result.stdout,result.stderr)
            elif result.returncode:assert "Assertion `!launches' failed" in result.stderr,result.stderr
            runs.append({'name':name,'display':mode,'sanitized':san,'returncode':result.returncode,
                         'status':'PASS' if result.returncode==0 else 'FAIL','log':log.name})
            print(name,mode,'ASan+UBSan' if san else 'normal',runs[-1]['status'],flush=True)
    # Reducer controls prove actual tap eligibility, including a full queue of
    # rapid same-millisecond DOWN/UP contacts, independently of Clock's no-tap UI.
    tap_source=ROOT/'test/native_apps/portable_touch_ordered_gt911_test.c'
    tap_exe=out/('taps-'+str(int(san)))
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-unused-function','-Wno-missing-field-initializers',*sanitizers,
                    '-DX4_GT911_FIXTURE='+json.dumps(str(physical)),
                    '-I'+str(inc),str(tap_source),str(driver),'-o',str(tap_exe)],check=True)
    for scenario in ('same-tick-recontacts','same-tick-burst'):
        result=subprocess.run([str(tap_exe),scenario],capture_output=True,text=True,timeout=15,
                              env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
        assert result.returncode==0,(scenario,result.stdout,result.stderr)
        log=out/(scenario+'-'+str(int(san))+'.log');log.write_text(result.stdout+result.stderr)
        runs.append({'name':scenario,'sanitized':san,'returncode':0,'status':'PASS','log':log.name})
        print(scenario,'ASan+UBSan' if san else 'normal','PASS',flush=True)
inputs=[*sources,tap_source,physical,manifest,ROOT/'test/native_apps/paper_clock_test.c',
        ROOT/'test/native_apps/paper_clock_ordered_fixture.h',
        ROOT/'lib/PortableApps/include/PortableTouch.h',Path(__file__).resolve()]
(out/'evidence.json').write_text(json.dumps({'runs':runs,'driver_version':a.driver_version,
    'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs},
    'staged_sdk_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(inc.glob('*.h'))},
    'hardware_tested':False,'production_modified':False},indent=2)+'\n')
if a.require_fixed:assert all(r['status']=='PASS' for r in runs),'Clock simultaneous-contact regression remains'
