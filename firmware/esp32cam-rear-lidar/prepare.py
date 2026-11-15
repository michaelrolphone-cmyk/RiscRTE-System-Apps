#!/usr/bin/env python3
"""Materialize the rear variant from checksum-pinned inherited camera sources."""
import argparse, hashlib, json, shutil, subprocess, tempfile, tarfile, io
from pathlib import Path
ROOT=Path(__file__).resolve().parent
UPSTREAM="14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7"
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--build',action='store_true');p.add_argument('--provisioning-json',type=Path,help='Private one-time factory Wi-Fi settings; never publish the generated image');a=p.parse_args()
 out=a.output.expanduser().absolute()
 source=ROOT.parents[1]
 revision='b92bb4bbd179027ae7ed378ef04fc17417f4265e'
 archive=subprocess.check_output(['git','-C',str(source),'archive',revision+':firmware/esp32cam-rtsp-radio'])
 inherited=tempfile.TemporaryDirectory(prefix='rear-camera-base-');base=Path(inherited.name)
 with tarfile.open(fileobj=io.BytesIO(archive)) as tree:
  for member in tree.getmembers():
   relative=Path(member.name)
   if relative.is_absolute() or '..' in relative.parts:raise ValueError('Unsafe inherited source path')
   if member.isfile():
    target=base/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(tree.extractfile(member).read())
 if out.exists() and any(out.iterdir()):raise ValueError('Destination must be empty')
 manifest=json.loads((ROOT/'manifest.json').read_text())
 for entry in manifest['inherited_files']:
  f=base/entry['path']
  if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=entry['sha256']:raise ValueError('Inherited source mismatch: '+entry['path'])
 out.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run(['git','clone','--quiet','--no-checkout','https://github.com/rzeldent/esp32cam-rtsp.git',str(out)],check=True)
 subprocess.run(['git','checkout','--quiet','--detach',UPSTREAM],cwd=out,check=True)
 subprocess.run(['git','apply','--check',str(base/'integration.patch')],cwd=out,check=True)
 subprocess.run(['git','apply','--whitespace=nowarn',str(base/'integration.patch')],cwd=out,check=True)
 for overlay in [base/'overlay',ROOT/'overlay']:
  for f in overlay.rglob('*'):
   if f.is_file():
    target=out/f.relative_to(overlay);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,target)
 board=out/'boards/esp32cam_s3_wroom_n16r8.json'
 definition=json.loads(board.read_text())
 definition['build']['extra_flags']=[flag.replace('ARDUINO_USB_CDC_ON_BOOT=1','ARDUINO_USB_CDC_ON_BOOT=0') for flag in definition['build']['extra_flags']]
 board.write_text(json.dumps(definition,indent=2))
 for name in ('ahrs_telemetry.cpp','ultrasonic_telemetry.cpp'):(out/'src'/name).unlink(missing_ok=True)
 settings=out/'include/settings.h';s=settings.read_text()
 import re
 s=re.sub(r'#define APP_TITLE .*','#define APP_TITLE "ROVER-REAR-CAMERA"',s)
 s=re.sub(r'#define APP_VERSION .*','#define APP_VERSION "1.0-rear.2"',s)
 s=re.sub(r'#define WIFI_SSID .*','#define WIFI_SSID "Rover-Rear-Camera"',s);settings.write_text(s)
 ini=out/'platformio.ini';s=ini.read_text()
 s=s.replace('build_flags =','board_build.partitions = partitions/rear-ota.csv\n\nbuild_flags =\n  -D VL53L5CX_NB_TARGET_PER_ZONE=1')
 s=s.replace('lib_deps =','lib_deps =\n  sparkfun/SparkFun VL53L5CX Arduino Library@1.0.3\n  links2004/WebSockets@2.7.2');ini.write_text(s)

 shutil.copyfile(source/'firmware/esp32cam-rtsp-radio/overlay/src/camera_websocket.inc',out/'src/camera_websocket.inc')
 subprocess.run(['git','apply','--whitespace=nowarn',str(ROOT/'camera-main.patch')],cwd=out,check=True)
 if a.provisioning_json:
  secret=json.loads(a.provisioning_json.expanduser().read_text())
  ssid=secret.get('ssid');password=secret.get('password')
  if not isinstance(ssid,str) or not 1<=len(ssid.encode())<=31 or not isinstance(password,str) or not 8<=len(password.encode())<=63:raise ValueError('Unsupported Wi-Fi credential lengths')
  header=out/'include/rear_factory_wifi.h'
  header.write_text('#pragma once\n#define REAR_FACTORY_SSID '+json.dumps(ssid)+'\n#define REAR_FACTORY_PASSWORD '+json.dumps(password)+'\n')
  header.chmod(0o600)
  main=out/'src/main.cpp';text=main.read_text()
  text='#include "rear_factory_wifi.h"\n#include <Preferences.h>\n'+text
  marker='  iotWebConf.init();'
  if text.count(marker)!=1:raise ValueError('Factory provisioning insertion point changed')
  text=text.replace(marker,marker+'''
  // Factory-only seed. Normal OTA images omit this code and retain saved settings.
  Preferences factory;
  if (factory.begin("rear-factory",false)) {
    if (!factory.getBool("seeded2",false)) {
      snprintf(iotWebConf.getWifiSsidParameter()->valueBuffer,IOTWEBCONF_WORD_LEN,"%s",REAR_FACTORY_SSID);
      snprintf(iotWebConf.getWifiPasswordParameter()->valueBuffer,IOTWEBCONF_PASSWORD_LEN,"%s",REAR_FACTORY_PASSWORD);
      snprintf(iotWebConf.getApPasswordParameter()->valueBuffer,IOTWEBCONF_PASSWORD_LEN,"%s",OTA_PASSWORD);
      strcpy(iotWebConf.getApTimeoutParameter()->valueBuffer,"0");
      iotWebConf.saveConfig();
      factory.putBool("seeded2",true);
      factory.end();
      ESP.restart();
    }
    factory.end();
  }
''')
  main.write_text(text)
  settings.write_text(settings.read_text().replace('"1.0-rear.2"','"1.0-rear.2-factory"'))

 print('Prepared rear variant:',out)
 if a.build:subprocess.run(['pio','run','-d',str(out),'-e','esp32cam_s3_wroom_n16r8'],check=True)
if __name__=='__main__':main()
