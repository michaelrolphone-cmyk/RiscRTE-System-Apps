#!/usr/bin/env python3
"""Real Settings save helper, old-service admission and verified RTC boundary."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/settings-clock-change';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 target=out/('test-san' if san else 'test');flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'Apps/settings.c'),str(root/'test/native_apps/settings_clock_change_test.c'),'-o',str(target)],check=True)
 subprocess.run([str(target)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
