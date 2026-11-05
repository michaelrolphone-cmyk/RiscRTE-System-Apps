#!/usr/bin/env python3
"""Qualify native USB app/raster/input/lifecycle and paper/feature-off parity.

Only capability providers are fixtures. No USB hardware behavior is inferred.
"""
import argparse, hashlib, json, os, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_portable_usb_transfer import FLAGS, NATIVE_FLAGS
SIZES=((160,160),(240,240),(240,280),(320,240),(410,502))
BASE='ff6151285ee41bdbeff75ebd31ee2621d524e899'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--msc-sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/usb-native')
 p.add_argument('--skip-baseline',action='store_true',help='Explicit partial run without pristine baseline parity')
 a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True);sdk=a.msc_sdk.resolve()
 header=sdk/'RiscUsbDeviceMscV1.h'
 if sha(header)!='d55a9340c3ff64e8679c4d9332812f073c30b8aa08ae3e3fb04f9010cf517ed6':raise ValueError('Qualification requires the pinned complete MSC diagnostics-v2 SDK')
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
 results=[];commands=[];traces={};captures=[]
 cc=os.environ.get('CC','cc')
 def run(cmd,**kw):
  commands.append(list(map(str,cmd)))
  return subprocess.run(cmd,check=True,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,**kw).stdout
 def build(root,target,flags,san=False,view=False,admission=False):
  extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  sources=[root/'test/native_apps/usb_native_view_test.c'] if view else [root/'Apps/usb_sd_transfer.c',root/'lib/PortableApps/src/adapter.c',root/('test/native_apps/usb_native_admission_test.c' if admission else 'test/native_apps/usb_transfer_test.c')]
  cmd=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*extra,*(['-ffunction-sections','-fdata-sections','-Wl,--gc-sections'] if view else []),'-I'+str(root/'lib/PortableApps/include'),'-I'+str(sdk),'-I'+str(root/'lib/NativeApps/include'),*map(str,sources),'-o',str(target)]
  run(cmd);return target
 with tempfile.TemporaryDirectory(prefix='usb-native-tests-') as scratch:
  tmp=Path(scratch)
  for san in (False,True):
   exe=build(ROOT,tmp/f'view-{san}',NATIVE_FLAGS+['-DPORTABLE_USB_TRANSFER_APP'],san,True)
   text=run([exe]);results.append({'kind':'view_hit','sanitized':san,'result':text.strip()});print(text,flush=True)
  for selected in (False,True):
   for w,h in ((159,160),(160,159),(160,160),(239,240),(240,239),(240,240),(320,160)):
    flags=FLAGS+(NATIVE_FLAGS if selected else [])+['-DTEST_USB_RGB',f'-DTEST_USB_NATIVE_WIDTH={w}',f'-DTEST_USB_NATIVE_HEIGHT={h}']
    label=f'admission-{w}x{h}-sel{int(selected)}'
    exe=build(ROOT,tmp/label,flags,admission=True)
    text=run([exe]);results.append({'kind':'admission','profile':label,'result':text.strip()})
   flags=FLAGS+(NATIVE_FLAGS if selected else [])+['-DTEST_NATIVE_LANDSCAPE']
   exe=build(ROOT,tmp/f'admission-paper-{selected}',flags,admission=True)
   text=run([exe]);results.append({'kind':'admission','profile':f'paper-sel{int(selected)}','result':text.strip()})
  print('16 selected/unselected color-boundary and paper admission cases passed',flush=True)
  profiles=[('native',w,h) for w,h in SIZES]+[('paper',480,800),('off',410,502)]
  for mode,w,h in profiles:
   for selected in ((False,True) if mode=='paper' else (mode=='native',)):
    for nav in (False,True):
     for snap in (False,True):
      for san in (False,True):
       label=f'{mode}-{w}x{h}-sel{int(selected)}-nav{int(nav)}-snap{int(snap)}-san{int(san)}'
       flags=[f for f in FLAGS if nav or f!='-DPORTABLE_INPUT_NAVIGATION']
       if selected:flags+=NATIVE_FLAGS
       flags+=['-DTEST_USB_TRACE']
       if snap:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
       if mode=='paper':flags+=['-DTEST_NATIVE_LANDSCAPE']
       else:flags+=['-DTEST_USB_RGB',f'-DTEST_USB_NATIVE_WIDTH={w}',f'-DTEST_USB_NATIVE_HEIGHT={h}']
       exe=build(ROOT,tmp/label,flags,san);log=[]
       for case in range(31):
        cmd=[exe,str(case)]
        capture=not san and not snap and not nav and mode=='native' and case in (0,2,3,4,5,11)
        if capture:
         frames=out/'frames'/label/str(case);frames.mkdir(parents=True,exist_ok=True);cmd.append(frames)
        text=run(cmd,timeout=20);log.append(text)
        trace=[line for line in text.splitlines() if line.startswith('FRAME ')]
        assert trace,(label,case,'no raster frames')
        key=(mode,w,h,selected,nav,case)
        if key in traces:assert trace==traces[key],(label,case,'direct/snapshot/sanitizer raster mismatch')
        else:traces[key]=trace
        if mode=='paper' and selected:
         assert trace==traces[(mode,w,h,False,nav,case)],(label,case,'selected paper raster changed')
        if capture:
         from PIL import Image
         for f in frames.glob('*.ppm'):
          png=f.with_suffix('.png');Image.open(f).save(png);captures.append({'path':str(png.relative_to(out)),'sha256':sha(png)});f.unlink()
       (out/(label+'.log')).write_text(''.join(log))
       results.append({'kind':'lifecycle','profile':label,'cases':31,'passed':True})
       print(label+': 31 cases passed',flush=True)
  if not a.skip_baseline:
   base=tmp/'base';base.mkdir()
   archive=subprocess.Popen(['git','archive',BASE],cwd=ROOT,stdout=subprocess.PIPE)
   subprocess.run(['tar','-x','-C',str(base)],stdin=archive.stdout,check=True);archive.stdout.close();assert archive.wait()==0
   # Only add a read-only frame digest to the baseline fixture, leaving all
   # assertions, events and production files byte-identical to the pinned tree.
   fixture=base/'test/native_apps/usb_transfer_test.c';text=fixture.read_text()
   anchor='*key=++presents;queued_until=ms+24;'
   text=text.replace(anchor,anchor+'\n uint64_t hash=UINT64_C(14695981039346656037);for(size_t i=0;i<sizeof(pixels);i++){hash^=pixels[i];hash*=UINT64_C(1099511628211);}printf("FRAME %u %016llx\\n",presents,(unsigned long long)hash);',1)
   fixture.write_text(text)
   for mode in ('paper','off'):
    for nav in (False,True):
     flags=[f for f in FLAGS if nav or f!='-DPORTABLE_INPUT_NAVIGATION']+(['-DTEST_USB_RGB'] if mode=='off' else ['-DTEST_NATIVE_LANDSCAPE'])
     exe=build(base,tmp/f'baseline-{mode}-{nav}',flags)
     for case in range(31):
      text=run([exe,str(case)],timeout=20);trace=[line for line in text.splitlines() if line.startswith('FRAME ')]
      size=(410,502) if mode=='off' else (480,800)
      assert trace==traces[(mode,*size,False,nav,case)],(mode,nav,case,'pristine feature-off parity')
     results.append({'kind':'baseline_parity','profile':mode,'navigation':nav,'cases':31,'passed':True})
     print(f'Pristine {mode} nav{int(nav)}:31 cases raster parity passed',flush=True)
 record={'schema':1,'scope':'source qualification; provider fixtures, no hardware claim','baseline_commit':BASE,'baseline_run':not a.skip_baseline,'sdk_sha256':sha(header),'results':results,'captures':captures,'source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [ROOT/'Apps/usb_sd_transfer.c',ROOT/'Apps/usb_sd_transfer_native.inc',ROOT/'Apps/UsbTransferLayout.h',ROOT/'Apps/UsbTransferNative.h',ROOT/'lib/PortableApps/src/adapter.c',ROOT/'lib/PortableApps/src/paper.inc',ROOT/'test/native_apps/usb_transfer_test.c',ROOT/'test/native_apps/paper_clock_test.c',ROOT/'test/native_apps/usb_native_view_test.c',ROOT/'test/native_apps/usb_native_admission_test.c',Path(__file__)]}}
 (out/'results.json').write_text(json.dumps(record,indent=2)+'\n');(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 print('Native USB complete matrix and unchanged paper/feature-off parity passed',flush=True)
if __name__=='__main__':main()
