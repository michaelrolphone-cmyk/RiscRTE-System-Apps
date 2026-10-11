#!/usr/bin/env python3
"""Link the transfer app to the actual shared USB provider and TinyUSB owner tests."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--reader',type=Path,required=True);p.add_argument('--tinyusb',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/usb-prepare-integration');a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'scripts'))
from build_portable_usb_transfer import FLAGS
stack=out/'stack'
if not stack.exists():subprocess.run([sys.executable,a.reader/'scripts/prepare_usb_device_stack.py','--source',a.tinyusb,'--output',stack],check=True)
include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True)
for name in ('RiscUsbDeviceMscV1.h','RiscStorageExportV1.h','RiscStorageVolumeV1.h','RiscStorageVolumePowerV1.h','RiscUsbPhyResourceV1.h'):
 source=a.reader/'sdk/driver'/name
 if source.exists():shutil.copyfile(source,include/name)
provider=a.reader/'Drivers/usb_device_msc_esp32s3'
results=[]
for sanitized in (False,True):
 binary=out/f'integration-{int(sanitized)}';flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 sources=[ROOT/'Apps/usb_sd_transfer.c',ROOT/'lib/PortableApps/src/adapter.c',ROOT/'test/native_apps/usb_prepare_integration_test.c',ROOT/'test/native_apps/usb_prepare_provider_bridge.c',provider/'driver.c',provider/'StackDefaults.c']+[stack/name for name in ('tusb.c','common/tusb_fifo.c','device/usbd.c','device/usbd_control.c','class/msc/msc_device.c')]
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*FLAGS,'-DTEST_NATIVE_LANDSCAPE',*flags,'-DUSB_OWNER_TEST_SOURCE="'+str(a.reader/'test/usb_device_msc/owner_test.c')+'"','-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(provider),'-I'+str(stack),'-I'+str(a.reader/'sdk/driver'),*map(str,sources),'-o',binary],check=True)
 for mode in range(4):
  result=subprocess.run([binary,str(mode)],check=True,timeout=20,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
  print(result.stdout.strip());results.append({'mode':mode,'sanitized':sanitized,'output':result.stdout.strip()})
(out/'evidence.json').write_text(json.dumps({'cases':results,'source_sha256':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in [*sources,a.reader/'test/usb_device_msc/owner_test.c',a.reader/'sdk/driver/RiscUsbDeviceMscV1.h',a.reader/'sdk/driver/RiscStorageExportV1.h',Path(__file__)]},'hardware_tested':False},indent=2)+'\n')
print('8 actual app/adapter/USB-provider/TinyUSB preparation cases passed')
