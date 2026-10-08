#!/usr/bin/env python3
"""Actual Watch/X4 update provider source selection, refusal and cleanup tests."""
import os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
policies=('watch','x4-configured','x4-unconfigured') if (ROOT/'Services/update/Policy.h').is_file() else ('watch',)
with tempfile.TemporaryDirectory(prefix='update-source-routes-') as temp:
 for sanitized in (False,True):
  for policy in policies:
   binary=Path(temp)/(policy+str(sanitized))
   flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
   if policy.startswith('x4'):flags+=['-DUPDATE_PRODUCT_X4']
   if policy=='x4-configured':flags+=['-DUPDATE_CATALOG_URL="https://raw.githubusercontent.com/michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO/release-index/release-index.json"']
   subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,
     *['-I'+str(ROOT/p) for p in ('lib/PortableApps/include','lib/NativeApps/include')],
     str(ROOT/'test/native_apps/update_source_routes_test.cpp'),'-o',str(binary)],check=True)
   for scenario in range(36):subprocess.run([str(binary),str(scenario)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print(str(72*len(policies))+' actual-provider source-route executions passed; profiles='+','.join(policies)+'; no physical network or flash')
