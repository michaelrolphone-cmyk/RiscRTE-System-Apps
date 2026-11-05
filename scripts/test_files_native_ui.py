#!/usr/bin/env python3
"""Actual Files adapter/controller/provider native dimensions and delayed input."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--snapshot',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'}
def run(cmd):subprocess.run(list(map(str,cmd)),check=True,env=env)
flags=['-g','-O1','-Wall','-Wextra','-Werror','-DPORTABLE_FILE_SHARING_SELECTED_VOLUME',*['-I'+str(ROOT/n) for n in ('lib/PortableApps/include','lib/NativeApps/include','lib/RemoteFiles','lib/UserFiles')],'-I'+str(ROOT.parent/'runtime/src'),'-I'+str(ROOT.parent/'runtime/sdk/app')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
objects=[]
sources=[ROOT/'lib/RemoteFiles'/ (n+'.cpp') for n in ('WebDavCore','WebDavVolume','WebDavProperties','WebDavSession','WebDavDigest','WebDavSharing','PortableFileSharing','WebDavClient','WebDavRemoteVolume','PortablePeerFiles')]
sources += [ROOT/n for n in ('lib/UserFiles/UserFilesVolume.cpp','test/remote_files/peer_ui_backend.cpp','lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c','lib/PortableApps/src/PortableNetworkSession.c','lib/PortableApps/src/PortableFileSetup.c')]
for source in sources:
 obj=a.output/(source.name+'.o');cpp=source.suffix=='.cpp';run(['c++' if cpp else 'cc','-std=c++17' if cpp else '-std=c11',*flags,'-Wno-misleading-indentation','-c',source,'-o',obj]);objects.append(obj)
text_flags=['-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT.parent/'runtime/sdk/driver')]
text_host=a.output/'text-host.o';run(['cc','-std=c11',*flags,*text_flags,'-I'+str(ROOT/'Services/text_input'),'-Dt5_driver_get=files_text_driver_get','-c',ROOT/'Services/text_input/host.c','-o',text_host])
selected=['-DPORTABLE_FILES_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_ASYNC_RGB565']
if a.snapshot:selected+=['-DPORTABLE_RASTER_SNAPSHOT']
results=[]
for w,h in ((160,160),(240,240),(320,180),(410,502),(502,410)):
 obj=a.output/f'native-{w}x{h}.o';exe=obj.with_suffix('');run(['cc','-std=c11',*flags,*selected,f'-DFILE_TEST_WIDTH={w}',f'-DFILE_TEST_HEIGHT={h}','-Wno-unused-function','-c',ROOT/'test/native_apps/files_native_ui_test.c','-o',obj]);run(['c++',*flags,obj,*objects,'-o',exe])
 for delay in (0,80):
  out=a.output/f'{w}x{h}-delay{delay}';out.mkdir(exist_ok=True);run([exe,out,delay]);results.append(dict(width=w,height=h,delay_ms=delay,passed=True))
 text_obj=a.output/f'text-{w}x{h}.o';text_exe=text_obj.with_suffix('');run(['cc','-std=c11',*flags,*selected,*text_flags,f'-DFILE_TEST_WIDTH={w}',f'-DFILE_TEST_HEIGHT={h}','-Wno-unused-function','-c',ROOT/'test/native_apps/file_browser_shared_input_test.c','-o',text_obj]);run(['c++',*flags,text_obj,text_host,*objects,'-o',text_exe])
 for scenario in ('entry-accept','entry-cancel','entry-home','entry-pending','entry-home-pending','local-rename','local-nonascii','local-filter','local-mkdir','local-cancel','local-home','local-invalid','local-long','local-unavailable','remote-rename','remote-actions','remote-mkdir','remote-cancel','remote-home','remote-invalid','remote-long','remote-unavailable','remote-expiry','local-retained'):
  out=a.output/f'text-{w}x{h}-{scenario}';out.mkdir(exist_ok=True);run([text_exe,scenario,out]);results.append(dict(width=w,height=h,text_scenario=scenario,passed=True))
 for fixture,extra,scenarios in (
  ('file_browser_sharing_test.c',['-DFILE_SETUP_TEST'],('ble-pair','ble-reject','ble-stale','ble-close-retry','ble-back-retry','ble-owner-loss','ble-expiry','ble-cancel-start')),
  ('file_browser_source_test.c',[],('local','local-secondary','local-tertiary','local-short','choose','full','unavailable','unsupported','begin-busy','cancel','busy-cancel','remote','remote-short','remote-cancel','remote-full','remote-interrupted','remote-reopen')),
  ('file_browser_direct_test.c',[],('complete','initial-on','refuse','cancel','expiry','active-expiry','no-peer','radio-fail','entropy-busy',*[f'stop-{i}' for i in range(36)]))):
  obj=a.output/f'{Path(fixture).stem}-{w}x{h}.o';exe=obj.with_suffix('');run(['cc','-std=c11',*flags,*selected,*extra,f'-DFILE_TEST_WIDTH={w}',f'-DFILE_TEST_HEIGHT={h}','-Wno-unused-function','-c',ROOT/'test/native_apps'/fixture,'-o',obj]);run(['c++',*flags,obj,*objects,'-o',exe])
  for scenario in scenarios:
   out=a.output/f'{Path(fixture).stem}-{w}x{h}-{scenario}';out.mkdir(exist_ok=True);run([exe,scenario,out]);results.append(dict(width=w,height=h,fixture=fixture,scenario=scenario,passed=True))
(a.output/'results.json').write_text(json.dumps(dict(sanitize=a.sanitize,snapshot=a.snapshot,cases=results,physical_hardware_verified=False),indent=2)+'\n')
