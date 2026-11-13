#!/usr/bin/env python3
"""Build owning Home dependency recipes; never rewrite scheduler or physical policy."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','watch','utilities','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--legacy-cc',required=True);p.add_argument('--native-cc',required=True)
 a=p.parse_args();a.output=a.output.resolve()
 if a.output.exists():raise ValueError('Fresh dependency output required')
 a.output.mkdir(parents=True)
 roots={'system':ROOT,'runtime':a.runtime.resolve(),'watch':a.watch.resolve(),'utilities':a.utilities.resolve()}
 heads={}
 for role,root in roots.items():
  subprocess.run(['git','-C',str(root),'diff','--exit-code','HEAD','--'],check=True)
  heads[role]=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
 env={**os.environ,'NATIVE_APP_CC':str(Path(a.legacy_cc).absolute())}
 recipes=[('canvas','build_canvas_services.py'),('civil','build_civil_clock.py'),('presentation','build_home_presentation.py')]
 for name,recipe in recipes:
  for suffix in ('','-repeat'):
   subprocess.run([sys.executable,str(ROOT/'scripts'/recipe),'--runtime',str(roots['runtime']),'--output',str(a.output/(name+suffix))],env=env,check=True)
 for suffix in ('','-repeat'):
  subprocess.run([sys.executable,str(roots['utilities']/'scripts/build_alarm_facade.py'),'--runtime',str(roots['runtime']),'--watch',str(roots['watch']),'--cc',str(Path(a.native_cc).absolute()),'--output',str(a.output/('alarm'+suffix))],check=True)
 packages={}
 selected={'raster':'canvas/canvas-raster','input':'canvas/canvas-input','civil-watch':'civil/watch','civil-x4':'civil/x4','presentation-faces':'presentation/faces','presentation-overview':'presentation/overview','alarm-legacy':'alarm/legacy-clock-change','alarm-tagged':'alarm/tagged-owner'}
 target=a.output/'packages';target.mkdir()
 for name,relative in selected.items():
  family,rest=relative.split('/',1);src=a.output/relative;repeat=a.output/(family+'-repeat')/rest
  for filename in ('driver.elf','manifest.json'):
   if (src/filename).read_bytes()!=(repeat/filename).read_bytes():raise ValueError('Repeated dependency differs '+name+'/'+filename)
  dest=target/name;dest.mkdir()
  for filename in ('driver.elf','manifest.json'):shutil.copyfile(src/filename,dest/filename)
  packages[name]={'recipe_output':relative,'elf_sha256':digest(dest/'driver.elf'),'elf_bytes':(dest/'driver.elf').stat().st_size,'manifest_sha256':digest(dest/'manifest.json')}
 # Input availability is deployment metadata; both variants use exactly the same
 # provider code, and neither acquires inputs during the raster-only TIMER path.
 for name,omitted in [('input-touch',{'input.navigation'}),('input-navigation',{'input.touch.raw'})]:
  dest=target/name;dest.mkdir();shutil.copyfile(target/'input/driver.elf',dest/'driver.elf')
  manifest=json.loads((target/'input/manifest.json').read_text())
  manifest['requires']=[r for r in manifest['requires'] if r['capability'] not in omitted]
  (dest/'manifest.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
  packages[name]={'same_code_as':'input','elf_sha256':digest(dest/'driver.elf'),'elf_bytes':(dest/'driver.elf').stat().st_size,'manifest_sha256':digest(dest/'manifest.json')}
 (a.output/'dependencies.json').write_text(json.dumps({'schema':1,'sources':heads,'packages':packages,'repeated_bytes_verified':True,'product_admission':False,'hardware_tested':False},sort_keys=True,indent=2)+'\n')
 print((a.output/'dependencies.json').read_text())
if __name__=='__main__':main()
