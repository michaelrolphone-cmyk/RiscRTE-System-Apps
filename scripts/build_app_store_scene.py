#!/usr/bin/env python3
"""Compile one app recipe, reproduce it, validate with selected Runtime, copy bytes."""
import argparse, hashlib, json, os, shutil, subprocess, tempfile
from pathlib import Path
from app_store_scene_support import stage
from scene_build import build, compiler_path
ROOT=Path(__file__).resolve().parents[1]
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 rt=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 sdk=stage(rt,ROOT,out/'sdk',a.utilities);cc=compiler_path();manifest_path=ROOT/'Apps/app_store_scene.json';m=json.loads(manifest_path.read_text())
 assert m['id']=='app_store' and m['file_name']=='app_store.elf'
 sources=[ROOT/'Apps/app_store_scene.c',ROOT/'Apps/app_store_inventory.c'];inputs={manifest_path,Path(__file__).resolve(),ROOT/'scripts/app_store_scene_support.py',ROOT/'scripts/scene_build.py',ROOT/'scripts/native_toolchain.py'}
 for src in sources:
  dependencies=subprocess.check_output([cc,'-MM','-I'+str(sdk),str(src)],text=True).replace('\\\n',' ').split(':',1)[1].split()
  inputs.update(Path(x).resolve() for x in dependencies)
 origins=json.loads((out/'sdk/sdk-origins.json').read_text());inputs.update(Path(x) for x in origins.values())
 before={str(x):digest(x) for x in sorted(inputs)}
 package=out/'packages/app_store';exports={'app_main','risc_resident_app_descriptor_v1'}
 first=build(cc,sdk,package,m,sources,exports=exports)
 with tempfile.TemporaryDirectory(prefix='repeat-',dir=out) as td:
  second=build(cc,sdk,Path(td),m,sources,exports=exports)
  assert first['sha256']==second['sha256'] and first['size_bytes']==second['size_bytes']
  assert (package/m['file_name']).read_bytes()==(Path(td)/m['file_name']).read_bytes()
 first['reproducibility']={'sha256':second['sha256'],'size_bytes':second['size_bytes'],'identical':True}
 validator_sources=[rt/'test/native_apps/validate_test.c',rt/'lib/elf_loader/src/esp_elf_validate.c'];validator=out/'validate'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2','-I'+str(rt/'lib/elf_loader/include'),'-I'+str(rt/'test/native_apps/stubs'),*map(str,validator_sources),'-o',str(validator)],check=True)
 run=subprocess.run([str(validator),str(package/m['file_name'])],check=True,capture_output=True,text=True);(out/'validation.log').write_text(run.stdout+run.stderr)
 first['selected_runtime_validator']={str(x):digest(x) for x in validator_sources+[rt/'test/native_apps/stubs/sdkconfig.h']+list((rt/'lib/elf_loader/include').rglob('*.h'))}
 selections={}
 for name in ('watch','wave','x4'):
  d=out/'selections'/name;d.mkdir(parents=True,exist_ok=True)
  for file in (m['file_name'],'app_store.json'):shutil.copyfile(package/file,d/file)
  assert digest(d/m['file_name'])==first['sha256']
  assert json.loads((d/'app_store.json').read_text())==m
  selections[name]={'sha256':digest(d/m['file_name']),'manifest_sha256':digest(d/'app_store.json')}
 assert all(digest(Path(x))==h for x,h in before.items())
 first['inputs']=before;first['selections']=selections;first['scope']='One profile-free Xtensa app recipe plus reproducibility rebuild, selected Runtime structural validation and three byte-identical selections; no target instruction execution, product integration or physical test.'
 (out/'receipt.json').write_text(json.dumps(first,indent=2)+'\n');print(first['sha256'],first['size_bytes'])
if __name__=='__main__':main()
