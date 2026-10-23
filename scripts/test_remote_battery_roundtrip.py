#!/usr/bin/env python3
"""Cross-repository actual codec, sensor parser and remote battery session."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--drivers',type=Path,required=True);p.add_argument('--output',type=Path,default=ROOT/'build/remote-battery-roundtrip');a=p.parse_args()
 a.output.mkdir(parents=True,exist_ok=True)
 canonical=a.drivers/'sdk/driver/RiscBluetoothSensorsV1.h'
 assert canonical.read_bytes()==(ROOT/'lib/PortableApps/include/RiscBluetoothSensorsV1.h').read_bytes()
 runs=[]
 for san in [False,True]:
  label='sanitized' if san else 'normal';binary=a.output/label
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(a.drivers),'-I'+str(a.drivers/'sdk/driver'),str(ROOT/'test/native_apps/remote_battery_roundtrip_test.c'),str(ROOT/'lib/PortableApps/src/remote_battery.c'),'-o',str(binary)]
  subprocess.run(cmd,check=True)
  r=subprocess.run([binary],check=True,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));print(label,r.stdout.strip());runs.append({'configuration':label,'command':cmd,'result':r.stdout.strip()})
 inputs=[a.drivers/'Drivers/ble_telemetry/codec.h',a.drivers/'Drivers/ble_sensors/ble_scan_core.h',canonical,ROOT/'lib/PortableApps/src/remote_battery.c',ROOT/'test/native_apps/remote_battery_roundtrip_test.c',ROOT/'test/native_apps/remote_battery_test.c']
 (a.output/'evidence.json').write_text(json.dumps({'runs':runs,'hardware_tested':False,'drivers_commit':subprocess.check_output(['git','-C',str(a.drivers),'rev-parse','HEAD'],text=True).strip(),'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2)+'\n')
if __name__=='__main__':main()
