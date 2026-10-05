#!/usr/bin/env python3
"""Offline product services/controller fault tests; no devices or credentials."""
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/portable/update-tests';OUT.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 binary=OUT/f'catalog-{sanitized}'
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/portable_update_catalog_test.cpp'),'-o',str(binary)],check=True)
 subprocess.run([str(binary),str(ROOT/'test/native_apps/fixtures/watch-release-index-38c5be065c69a493bfc0d075fcf76ecd9b99450e.json')],check=True,timeout=20)
 for firmware in (0,1):
  binary=OUT/f'service-{firmware}-{sanitized}'
  subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,f'-DUPDATE_FIRMWARE={firmware}','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/portable_update_service_test.cpp'),'-o',str(binary)],check=True)
  for scenario in range(20): subprocess.run([str(binary),str(scenario)],check=True,timeout=15)
 for firmware,rotation,nova in ((f,r,n) for f in (0,1) for r in (0,180) for n in (False,True)):
  binary=OUT/f'app-{firmware}-{rotation}-{nova}-{sanitized}'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11',f'-DPORTABLE_UPDATE_FIRMWARE={firmware}','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,*(['-DPORTABLE_NOVA_UI'] if nova else []),f'-DPORTABLE_TOUCH_ROTATION={rotation}','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/portable_update_app_test.c'),'-o',str(binary)],check=True)
  for scenario in range(38): subprocess.run([str(binary),str(scenario)],check=True,timeout=15)
print('Portable updates: catalog mutations; 20 service scenarios x 2 kinds x 2 modes; 38 UI scenarios x 2 kinds x 2 orientations x 2 views x 2 modes passed')
