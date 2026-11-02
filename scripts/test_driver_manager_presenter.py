#!/usr/bin/env python3
"""Run task-owned semantic Driver Manager tests, including sanitizers."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
for mode in ('normal','sanitized'):
 exe=a.output/mode
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'sdk/app'),str(root/'test/driver_manager/presenter_test.c'),'-o',str(exe)],check=True)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
 run=subprocess.run([str(exe.resolve())],env=env,text=True,capture_output=True)
 (a.output/(mode+'.log')).write_text(run.stdout+run.stderr)
 print(run.stdout+run.stderr,flush=True);run.check_returncode()
