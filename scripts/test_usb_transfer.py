#!/usr/bin/env python3
"""Run actual app/adapter/provider-callback USB transfer lifecycle regressions."""
import argparse,os,subprocess,sys
from PIL import Image
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--msc-sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/usb-transfer');a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'scripts'))
from build_portable_usb_transfer import FLAGS
for san in (False,True):
 target=a.output_dir/f'usb-transfer-{int(san)}'
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*(['-no-pie'] if sys.platform!='darwin' else [])] if san else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*FLAGS,'-DTEST_NATIVE_LANDSCAPE',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(a.msc_sdk),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/usb_sd_transfer.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/usb_transfer_test.c'),'-o',str(target)],check=True)
 for case in range(31 if "RISC_USB_MSC_DIAGNOSTICS_TAG" in (a.msc_sdk/"RiscUsbDeviceMscV1.h").read_text() else 18):
  frames=a.output_dir/f'frames-{int(san)}-{case}';frames.mkdir(exist_ok=True)
  subprocess.run([str(target),str(case),str(frames)],check=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
  if not san:
   for file in frames.glob('*.pbm'):Image.open(file).rotate(270,expand=True).save(file.with_suffix('.png'))
print('Normal/ASan/UBSan production app + adapter USB lifecycle and protocol diagnostic cases passed')
