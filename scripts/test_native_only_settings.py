#!/usr/bin/env python3
import argparse,pathlib,subprocess,tempfile,os
from build_portable_settings import native_time_sdk,stage_native_time_sdk
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=pathlib.Path);a=p.parse_args()
class Options: native_time_runtime_repo=a.runtime
with tempfile.TemporaryDirectory() as tmp:
 out=pathlib.Path(tmp);inc=stage_native_time_sdk(out,native_time_sdk(Options,p));source=root/'test/native_apps/native_only_settings_test.c'
 for san in [[],['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']]:
  cmd=['cc','-std=c11','-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*san,'-I'+str(inc),'-I'+str(root/'lib/NativeApps/include'),str(source),*[str(root/'lib/PortableApps/src'/n) for n in ['PortableSetTime.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']],'-o',str(out/'check')]
  subprocess.run(cmd,check=True)
  for mode in ['ok','io','read-io']:subprocess.run([str(out/'check'),mode],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
