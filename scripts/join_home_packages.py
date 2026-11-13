#!/usr/bin/env python3
"""Join exact same-run dependency packages without silent overwrites."""
import argparse,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
EXPECTED={
 'built':{'raster','input','input-touch','input-navigation','civil-watch','civil-x4','presentation-faces','presentation-overview','alarm-legacy','alarm-tagged'},
 'published':{'scene-full','scene-color-dynamic','scene-watch-compact','contexts'},
 'x4':{'x4-power','x4-display-power','x4-physical-power'},
 'native':{'native-watch','native-wave','native-x4'}}
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in (*EXPECTED,'output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args()
 if a.output.exists():raise ValueError('Fresh package join required')
 built=json.loads((a.built/'dependencies.json').read_text())
 head=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
 if built['sources']['system']!=head or built['repeated_bytes_verified'] is not True:raise ValueError('Same-source repeated dependency build required')
 roots={name:getattr(a,name)/'packages' for name in EXPECTED}
 for name,root in roots.items():
  if set(x.name for x in root.iterdir())!=EXPECTED[name] or any(not x.is_dir() or x.is_symlink() for x in root.iterdir()):raise ValueError('Exact package family differs '+name)
  if any(x.is_symlink() for x in root.rglob('*')):raise ValueError('Package symlink forbidden')
 a.output.mkdir(parents=True)
 for root in roots.values():
  for package in root.iterdir():shutil.copytree(package,a.output/package.name)
 print('Distinct exact dependency packages joined; no acceptance inferred')
if __name__=='__main__':main()
