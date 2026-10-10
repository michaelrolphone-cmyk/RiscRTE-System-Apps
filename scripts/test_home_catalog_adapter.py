#!/usr/bin/env python3
"""Check the selected existing-grant Home catalog projection and custody fence."""
import argparse,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--utilities',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as directory:
 for sanitized in (False,True):
  flags=['-std=c11','-Wall','-Wextra','-Werror','-DPORTABLE_ALARM_TERMINAL_RETENTION']
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  exe=Path(directory)/str(sanitized)
  subprocess.run(['cc',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(a.utilities/'lib/Alarm/include'),str(root/'test/native_apps/home_catalog_adapter_test.c'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
