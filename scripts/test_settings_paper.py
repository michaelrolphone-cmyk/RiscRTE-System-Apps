#!/usr/bin/env python3
import os,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1];out=root/'build/paper-settings';out.mkdir(parents=True,exist_ok=True)
exe=out/'settings'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'Apps/settings.c'),str(root/'test/native_apps/portable_settings_paper_test.c'),'-o',str(exe)],check=True)
for scenario in [0,1,2,3,4,5,6,7,8,9,10,12,13,15]:subprocess.run([str(exe),str(scenario)],check=True,timeout=10)
