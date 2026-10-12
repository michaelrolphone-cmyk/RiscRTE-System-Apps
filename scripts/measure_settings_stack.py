#!/usr/bin/env python3
"""Measure selected Settings caller frames; this is not a complete stack budget."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
import build_portable_settings as builder
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.target=a.target.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
r=json.loads((a.target/'settings-build-record.json').read_text())
assert r.get('atomic_settings',{}).get('namespace')==1
includes=a.target/'native-time-sdk/include'
cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
if not cc:
 core=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))
 cc=str(core/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
sources=['Apps/settings_native_entry.c','lib/PortableApps/src/adapter.c',*builder.NATIVE_TIME_SOURCES]
for src in sources:
 subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden',
  '-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-Wall','-Wextra','-Werror','-fstack-usage',
  *r['build_defines'],'-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),'-c',str(ROOT/src),
  '-o',str(a.output/(Path(src).stem+'.o'))],check=True)
frames=[]
for su in sorted(a.output.glob('*.su')):
 for line in su.read_text().splitlines():
  source,size,kind=line.split('\t');frames.append({'source':source,'bytes':int(size),'kind':kind})
frames.sort(key=lambda row:-row['bytes'])
receipt={'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
 'elf_sha256':r['sha256'],'scope':'per-function caller frames only; no native broker, RTOS/loader, interrupt or libc call-chain bound',
 'policy_changed':False,'frames':frames}
(a.output/'stack-usage.json').write_text(json.dumps(receipt,indent=2)+'\n')
for frame in frames:
 if any(s in frame['source'] for s in ['settings_atomic','preference_atomic','settings_activate','snt_save','app_main']):print(frame)
