#!/usr/bin/env python3
import pathlib,tempfile,subprocess,os
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 for profile in [[],['-DTEST_SETTINGS_VIEW'],['-DTEST_WIFI_VIEW']]:
  for san in [[],['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']]:
   binary=pathlib.Path(d)/'test';subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*san,*profile,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'test/native_apps/safe_viewport_raster_test.c'),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
