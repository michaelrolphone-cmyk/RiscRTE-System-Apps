#!/usr/bin/env python3
"""Model full-speed packet timing through actual USB app/adapter/MSC/SD/FatFs.
No model duration is a hardware speed measurement. No device is accessed.
"""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ('reader','runtime','x4','tinyusb','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--app-source',type=Path,default=ROOT/'Apps/usb_sd_transfer.c')
p.add_argument('--sanitized',action='store_true');p.add_argument('--host-gaps',type=int,nargs='+',default=[0,50,200,500,1000,2000,10000]);a=p.parse_args()
out=a.output.resolve();assert not out.exists(),'Use a new output directory';out.mkdir(parents=True)
subprocess.run([sys.executable,a.reader/'scripts/prepare_usb_device_stack.py','--source',a.tinyusb,'--output',out/'stack'],check=True)
subprocess.run([sys.executable,a.x4/'minimal/scripts/prepare_sdk.py','--runtime',a.runtime,'--reader',a.reader,'--output',out/'sdk'],check=True)
from build_portable_usb_transfer import FLAGS
sources=[a.app_source,ROOT/'lib/PortableApps/src/adapter.c',ROOT/'test/native_apps/usb_cadence_app_test.c',ROOT/'test/native_apps/usb_cadence_provider_bridge.c',a.reader/'test/usb_device_msc/sd_directory_bridge.c',a.reader/'Drivers/usb_device_msc_esp32s3/driver.c',a.reader/'Drivers/usb_device_msc_esp32s3/StackDefaults.c',a.reader/'Drivers/storage_fatfs/fatfs/ff.c',a.reader/'Drivers/storage_fatfs/fatfs/ffunicode.c']+[out/'stack'/n for n in ('tusb.c','common/tusb_fifo.c','device/usbd.c','device/usbd_control.c','class/msc/msc_device.c')]
san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitized else []
base=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-overflow',*san,*FLAGS,'-DTEST_NATIVE_LANDSCAPE','-DUSB_OWNER_TEST_SOURCE="'+str(a.reader/'test/usb_device_msc/owner_test.c')+'"']
common=[ROOT/'Apps',ROOT/'lib/NativeApps/include',ROOT/'test/native_apps',a.reader/'Drivers/usb_device_msc_esp32s3',out/'stack',a.reader/'sdk/driver',a.x4/'minimal/test',a.reader/'Drivers/storage_fatfs',a.reader/'Drivers/x4pro_board',a.reader]
commands=[];objects=[];deps={}
for i,source in enumerate(sources):
 inc=[ROOT/'lib/PortableApps/include',out/'sdk'] if i<3 else [out/'sdk',ROOT/'lib/PortableApps/include']
 obj=out/(str(i)+'.o');dep=out/(str(i)+'.d');line=base+['-I'+str(p) for p in [*inc,*common]]+['-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)];subprocess.run(line,check=True);commands.append(line);objects.append(str(obj))
 for n in dep.read_text().replace('\\\n',' ').split(':',1)[1].split():q=Path(n).resolve();deps[str(q)]=hashlib.sha256(q.read_bytes()).hexdigest()
line=['cc',*san,*objects,'-o',str(out/'test')];subprocess.run(line,check=True);commands.append(line);rows=[]
for gap in a.host_gaps:
 r=subprocess.run([out/'test',str(gap)],capture_output=True,text=True,timeout=60,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
 print(r.stdout,end='');print(r.stderr,end='',file=sys.stderr);assert not r.returncode,(gap,r.returncode);row=json.loads(r.stdout.splitlines()[-1]);assert row['sd_reads']==293 and row['commands']==167 and row['packets']==2678 and row['unchanged_card'];rows.append(row)
record={'hardware_tested':False,'timing':'Deterministic65 us full-speed packet/DCD timing; configured host gaps are assumptions. Actual app/adapter/MSC/TinyUSB/SD/FatFs. Display, touch, physical DCD/SDMMC and scheduler clock are modeled.','sanitized':a.sanitized,'app_source':str(a.app_source),'commands':commands,'compiled_dependencies_sha256':deps,'cases':rows}
(out/'measurement.json').write_text(json.dumps(record,indent=2)+'\n')
