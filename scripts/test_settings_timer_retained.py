#!/usr/bin/env python3
"""Actual Settings custody wrapper and mode/idle/deep save fences."""
import argparse,os,subprocess,tempfile
from pathlib import Path
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument("--native-settings-ui",action="store_true");p.add_argument("--runtime-sdk",type=Path,required=True);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='settings-timer-retained-') as tmp:
  for san,asynchronous in [(s,a) for s in [False,True] for a in [False,True]]:
   flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
   if a.native_settings_ui:flags+=['-DPORTABLE_SETTINGS_NATIVE_GEOMETRY','-DPORTABLE_SAFE_VIEWPORT','-DTEST_SETTINGS_RASTER_WIDTH=466','-DTEST_SETTINGS_RASTER_HEIGHT=466','-DTEST_SETTINGS_INSET=68']
   if asynchronous: flags+=['-DTEST_SETTINGS_ASYNC']
   exe=Path(tmp)/'test';subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(S/'lib/PortableApps/include'),'-I'+str(S/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk),str(S/'Apps/settings.c'),str(S/'test/native_apps/settings_timer_retained_test.c'),'-o',str(exe)],check=True)
   for kind in range(3):
    for interruption in (range(5) if asynchronous else [0]):subprocess.run([str(exe),str(kind),str(interruption)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
if __name__=='__main__':main()
