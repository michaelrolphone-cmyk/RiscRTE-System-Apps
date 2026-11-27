#!/usr/bin/env python3
"""Build one generic canvas provider ELF; choose input grants in package metadata."""
import argparse,hashlib,json,pathlib,shutil,subprocess,tempfile
from scene_build import build,compiler_path,json_write
from scene_sdk import stage_sdk
from normalize_xtensa_relocations import normalize
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run(runtime,output):
 output.mkdir(parents=True,exist_ok=False)
 compiler=compiler_path()
 manifest=json.loads((ROOT/'Services/canvas_host/manifest.json').read_text())
 with tempfile.TemporaryDirectory(prefix='canvas-sdk-') as temporary:
  include=stage_sdk(runtime,ROOT,pathlib.Path(temporary))
  shutil.copyfile(ROOT/'sdk/app/RiscCanvasV1.h',include/'RiscCanvasV1.h')
  libgcc=pathlib.Path(subprocess.check_output([compiler,'-print-libgcc-file-name'],text=True).strip())
  if not libgcc.is_file():raise FileNotFoundError('Matching compiler support archive missing')
  support=pathlib.Path(temporary)/'libgcc.a';shutil.copyfile(libgcc,support)
  source=ROOT/'Services/canvas_host/host.c'
  row=build(compiler,include,output/'canvas-host',manifest,[source,support])
  repeat=build(compiler,include,output/'repeat',manifest,[source,support])
  if row['sha256']!=repeat['sha256']:raise ValueError('Repeated raw canvas ELF differs')
  for folder, receipt in [('canvas-host', row), ('repeat', repeat)]:
   elf=output/folder/'driver.elf'
   shutil.copyfile(elf,output/folder/'driver.linker-original.elf')
   receipt['linker_original_sha256']=receipt['sha256']
   receipt['removed_trailing_none_relocations']=normalize(elf)
   receipt['sha256']=hashlib.sha256(elf.read_bytes()).hexdigest()
   json_write(output/folder/'build.json',receipt)
  if row['sha256']!=repeat['sha256']:raise ValueError('Repeated normalized canvas ELF differs')
  row['source_sha256'].update({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Services/canvas_host').glob('*.h')})
  row['build_inputs_sha256']={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [libgcc,ROOT/'scripts/scene_build.py',ROOT/'scripts/scene_sdk.py',ROOT/'scripts/normalize_xtensa_relocations.py',ROOT/'Services/canvas_host/manifest.json',pathlib.Path(__file__)]}
  row['repeat_sha256']=repeat['sha256'];row['content_encodings']={'MONO1':'MONO1 or RGB565 physical surfaces','RGBA8':'RGB565 only; source-over white in sRGB channel space'}
  json_write(output/'canvas-host/build.json',row)
  validator=pathlib.Path(temporary)/'validate-elf'
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
  subprocess.run([str(validator),str(output/'canvas-host/driver.elf')],check=True)
  variants={}
  for name,omitted in [('touch',{'input.navigation'}),('display-only',{'input.navigation','input.touch.raw'})]:
   variant=dict(manifest);variant['requires']=[r for r in manifest['requires'] if r['capability'] not in omitted]
   json_write(output/f'manifest-{name}.json',variant);variants[name]=row['sha256']
  json_write(output/'packages.json',{'schema':1,'provider':row,'same_elf_optional_input_variants':variants,'physical_testing':'not performed'})
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();run(a.runtime.resolve(),a.output.resolve())
