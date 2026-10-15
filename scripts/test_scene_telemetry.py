#!/usr/bin/env python3
"""Compile and exercise the actual scene host; no physical display required."""
import argparse,os,subprocess,tempfile
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def run(runtime):
 with tempfile.TemporaryDirectory(prefix='scene-telemetry-') as tmp:
  out=Path(tmp);inc=stage_sdk(runtime,ROOT,out)
  for sanitized in (False,True):
   exe=out/('sanitized' if sanitized else 'plain')
   flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(inc)]
   if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   for test in ('telemetry_test.c','landscape_test.c'):
    subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'test/scene/telemetry_probe.c'),str(ROOT/'test/scene'/test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=60)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);a=p.parse_args();run(a.runtime.resolve())
