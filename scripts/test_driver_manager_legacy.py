#!/usr/bin/env python3
"""Preserved ordinary-package parser tests; not installation qualification."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
for source in sorted((root/'test/driver_manager/legacy').glob('*.cpp')):
 for mode in ('normal','sanitized'):
  name=source.stem+'-'+mode;exe=a.output/name
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
  subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror',*flags,
   '-I'+str(root/'Apps/driver_manager/legacy/include'),'-I'+str(a.runtime/'src/runtime/packages'),
   str(source),'-lcrypto','-o',str(exe)],check=True)
  run=subprocess.run([str(exe.resolve())],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'),text=True,capture_output=True)
  (a.output/(name+'.log')).write_text(run.stdout+run.stderr);print(name+': '+run.stdout+run.stderr,flush=True);run.check_returncode()
