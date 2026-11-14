#!/usr/bin/env python3
"""Materialize the rear variant from checksum-pinned inherited camera sources."""
import argparse, hashlib, json, shutil, subprocess, tempfile, tarfile, io
from pathlib import Path
ROOT=Path(__file__).resolve().parent
UPSTREAM="14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7"
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--build',action='store_true');a=p.parse_args()
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
 for name in ('ahrs_telemetry.cpp','ultrasonic_telemetry.cpp'):(out/'src'/name).unlink(missing_ok=True)
 settings=out/'include/settings.h';s=settings.read_text()
 import re
 s=re.sub(r'#define APP_TITLE .*','#define APP_TITLE "ROVER-REAR-CAMERA"',s)
 s=re.sub(r'#define APP_VERSION .*','#define APP_VERSION "1.0-rear.1"',s)
 s=re.sub(r'#define WIFI_SSID .*','#define WIFI_SSID "Rover-Rear-Camera"',s);settings.write_text(s)
 ini=out/'platformio.ini';s=ini.read_text()
 s=s.replace('build_flags =','board_build.partitions = partitions/rear-ota.csv\n\nbuild_flags =\n  -D VL53L5CX_NB_TARGET_PER_ZONE=1')
 s=s.replace('lib_deps =','lib_deps =\n  sparkfun/SparkFun VL53L5CX Arduino Library@1.0.3');ini.write_text(s)
 print('Prepared rear variant:',out)
 if a.build:subprocess.run(['pio','run','-d',str(out),'-e','esp32cam_s3_wroom_n16r8'],check=True)
if __name__=='__main__':main()
