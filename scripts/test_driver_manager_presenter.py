#!/usr/bin/env python3
"""Run task-owned Driver Manager model/selection tests including sanitizers."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
for test in ('presenter','selection','controller'):
 for mode in ('normal','sanitized'):
  name=test+'-'+mode;exe=a.output/name
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
  includes=[root/'sdk/app',root/'Apps/driver_manager',a.runtime/'sdk/driver',a.runtime/'sdk/app']
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',*flags,*['-I'+str(x) for x in includes],str(root/('test/driver_manager/'+test+'_test.c')),'-o',str(exe)],check=True)
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  run=subprocess.run([str(exe.resolve())],env=env,text=True,capture_output=True)
  (a.output/(name+'.log')).write_text(run.stdout+run.stderr)
  print(name+': '+run.stdout+run.stderr,flush=True);run.check_returncode()
