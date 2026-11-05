#!/usr/bin/env python3
"""Pixel parity for all non-editor Files screens, against the default profile."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--objects',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
objects=[x for x in a.objects.glob('*.o') if x.name not in ('rgb.o','paper.o','paper-scroll.o','text-host.o')]
assert len(objects)==15,len(objects)
flags=['-g','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-DPORTABLE_FILE_SHARING_SELECTED_VOLUME',*['-I'+str(ROOT/n) for n in ['lib/PortableApps/include','lib/NativeApps/include','lib/RemoteFiles','lib/UserFiles']],'-I'+str(ROOT.parent/'runtime/src'),'-I'+str(ROOT.parent/'runtime/sdk/app')]
results=[]
for profile in ('rgb','paper','paper-scroll'):
 selected=[] if profile=='rgb' else ['-DFILE_BROWSER_PAPER_PROFILE']
 if profile=='paper-scroll':selected+=['-DPORTABLE_TOUCH_SCROLL']
 for variant in ('legacy','shared'):
  output=a.output/(profile+'-'+variant);output.mkdir();obj=output/'fixture.o'
  extra=['-DPORTABLE_FILE_BROWSER_SHARED_INPUT','-DPORTABLE_TEXT_INPUT_CLIENT'] if variant=='shared' else []
  subprocess.run(['cc','-std=c11',*flags,*selected,*extra,'-c',str(ROOT/'test/native_apps/file_browser_ui_parity_test.c'),'-o',str(obj)],check=True)
  subprocess.run(['c++',str(obj),*map(str,objects),'-o',str(output/'test')],check=True)
  subprocess.run([str(output/'test'),str(output)],check=True)
 before={x.name:x.read_bytes() for x in (a.output/(profile+'-legacy')).glob('*.raw')};after={x.name:x.read_bytes() for x in (a.output/(profile+'-shared')).glob('*.raw')}
 assert before==after and len(before)==14,(profile,len(before),[n for n in before if before[n]!=after.get(n)])
 for name,raw in before.items():results.append(dict(profile=profile,screen=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),byte_identical=True))
(a.output/'pixel-parity.json').write_text(json.dumps(dict(screens=results,total=len(results),physical_hardware_verified=False),indent=2)+'\n');print('42 non-editor screen byte comparisons passed')
