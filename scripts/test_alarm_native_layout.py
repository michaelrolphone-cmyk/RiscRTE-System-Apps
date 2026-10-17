#!/usr/bin/env python3
"""Bound native alarm rectangles across the uint16 display ABI, without pixels."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);rows=[]
for sanitized in (False,True):
    exe=a.output/'layout-test'
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
         '-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'test/native_apps/alarm_native_layout_test.c'),'-o',str(exe)]
    built=subprocess.run(cmd,text=True,capture_output=True)
    (a.output/f'build-{int(sanitized)}.log').write_text(built.stdout+built.stderr);built.check_returncode()
    run=subprocess.run([str(exe)],text=True,capture_output=True,timeout=30,
        env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    (a.output/f'run-{int(sanitized)}.log').write_text(run.stdout+run.stderr);run.check_returncode()
    rows.append({'sanitized':sanitized,'passed':True,**json.loads(run.stdout),
        'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()});exe.unlink()
paths=['lib/PortableApps/include/PortableAlarmLayout.h','test/native_apps/alarm_native_layout_test.c','scripts/test_alarm_native_layout.py']
(a.output/'results.json').write_text(json.dumps({'rows':rows,'scope':'Pure rectangle and hit boundaries, not renderer or hardware acceptance',
    'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths}},indent=2)+'\n')
print(json.dumps(rows))
