#!/usr/bin/env python3
from pathlib import Path
import argparse,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--app-data-sdk',type=Path,required=True);a=p.parse_args()
out=ROOT/'build/broadcast-app-data';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(a.app_data_sdk)]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/f'fence-{int(san)}'
 subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'test/native_apps/broadcast_app_data_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
