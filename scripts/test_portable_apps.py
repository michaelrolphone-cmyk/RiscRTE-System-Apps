#!/usr/bin/env python3
import argparse,os,subprocess,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--utilities',type=Path);a=p.parse_args()
for name,pin in json.loads((ROOT/'lib/PortableApps/SOURCES.json').read_text()).items():
 assert hashlib.sha256((ROOT/'lib/PortableApps/include'/name).read_bytes()).hexdigest()==pin['sha256'], name
out=ROOT/'build/portable';out.mkdir(parents=True,exist_ok=True)
for name,source,flags in [('springboard',ROOT/'Apps/springboard.c',[])]+([('battery',a.utilities.resolve()/'Apps/battery.c',['-DBATTERY_TEST'])] if a.utilities else []):
 binary=out/name
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(source),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/portable_adapter_test.c'),'-o',str(binary)],check=True,timeout=60)
 for n in range(8 if name == "battery" else 5):subprocess.run([str(binary),str(n)],check=True,timeout=10)
