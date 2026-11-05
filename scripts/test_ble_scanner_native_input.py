#!/usr/bin/env python3
"""Production private BLE Scanner ownership reducer, normal and ASan/UBSan."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/ble-scanner-native-input';out.mkdir(parents=True,exist_ok=True)
for sanitize in (False,True):
    exe=out/('sanitized' if sanitize else 'normal')
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/PortableApps/src'),str(root/'test/native_apps/ble_scanner_native_input_test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
