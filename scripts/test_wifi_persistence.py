#!/usr/bin/env python3
"""Selected X4 Wi-Fi controller + production native NVS adapter with disk-backed SDK faults."""
import argparse,json,subprocess,os,hashlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];r=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);commands=[]
def run(cmd,log):
 cmd=list(map(str,cmd));commands.append(cmd);x=subprocess.run(cmd,capture_output=True,text=True,timeout=90);log.write_text(x.stdout+x.stderr)
 if x.returncode:print(x.stdout+x.stderr);x.check_returncode()
for mode in ('normal','ubsan'):
 d=out/mode;d.mkdir(exist_ok=True);store=d/'synthetic-nvs.bin'
 if store.exists():store.unlink()
 san=[] if mode=='normal' else ['-fsanitize=undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 run([os.environ.get('CC','cc'),'-std=c11','-DTEST_IDLE_ELIGIBILITY','-Wall','-Wextra','-Werror','-Wno-unused-function',*san,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),'-I'+str(r/'sdk/app'),'-c',root/'test/native_apps/wifi_persistence_test.c','-o',d/'app.o'],d/'compile-c.log')
 inc=['test/native_nvs_shim','src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src']
 run([os.environ.get('CXX','c++'),'-std=c++17',*san,*['-I'+str(r/x) for x in inc],root/'test/native_apps/wifi_persistence_nvs.cpp',d/'app.o','-o',d/'test'],d/'compile-cxx.log')
 for case in ('write','reboot','fault','snapshot'):
  run([d/'test',case,store],d/(case+'.log'))
 print(mode,'two-profile write/reentry/cold-process/edit/delete/fault PASS',flush=True)
files=[root/'Apps/wifi_settings_portable.inc',root/'test/native_apps/wifi_persistence_test.c',root/'test/native_apps/wifi_persistence_nvs.cpp',r/'src/ports/esp32s3/NvsKeyValue.h',root/'lib/PortableApps/include/PortableWifiProfiles.h',root/'lib/PortableApps/include/PortableWifiCredentials.h']
(out/'receipt.json').write_text(json.dumps({'commands':commands,'inputs':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'normal':True,'ubsan':True,'namespace':6,'native_namespace':'rte00000006','cold_restart':'Separate process, disk-backed synthetic NVS; not hardware flash durability','boundaries':'Production app/profile codec/native NVS adapter. Synthetic radio, outer grants, peripherals, and NVS SDK. Selected boot grants audited separately.'},indent=2)+'\n')
