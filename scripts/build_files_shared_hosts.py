#!/usr/bin/env python3
"""Build only the backward-compatible extended text/scene hosts for Files."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
from scene_sdk import stage_sdk
from scene_build import build,compiler_path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--target',choices=('watch','x4'),required=True);p.add_argument('--watch-product',type=Path);a=p.parse_args()
if a.target=='watch' and not a.watch_product:p.error('--watch-product is required for the accepted metadata-only compaction recipe')
if a.target=='x4':
 font=ROOT/'Services/scene_host/fonts/telemetry.inc'
 if not font.exists():subprocess.run([sys.executable,str(ROOT/'scripts/generate_telemetry_fonts.py')],check=True)
 assert hashlib.sha256(font.read_bytes()).hexdigest()=='2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1','Existing telemetry typography differs'
assert not a.output.exists();a.output.mkdir(parents=True)
inc=stage_sdk(a.runtime.resolve(),ROOT,a.output/'sdk');shutil.copyfile(ROOT/'sdk/app/RiscTextEntryV1.h',inc/'RiscTextEntryV1.h')
for name in ('RiscUsbHidV1.h','RiscUsbControllerV1.h','RiscUsbProviderV1.h'):shutil.copyfile(ROOT/'Services/text_input'/name,inc/name)
cc=compiler_path();rows={}
for name,source,flags in [('scene','scene_host',['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1'] if a.target=='watch' else []),('text','text_input',[])]:
 manifest_name='manifest-watch-compact.json' if name=='scene' and a.target=='watch' else 'manifest.json'
 manifest=json.loads((ROOT/'Services'/source/manifest_name).read_text());out=a.output/name
 row=build(cc,inc,out,manifest,[ROOT/'Services'/source/'host.c'],flags)
 if a.target=='watch':
  sys.path.insert(0,str(a.watch_product/'scripts'));from compact_current_elf import compact
  row['compaction']=compact(out/'driver.elf',cc,debug_path=out/'debug/driver.elf')
  row['sha256']=hashlib.sha256((out/'driver.elf').read_bytes()).hexdigest();row['size_bytes']=(out/'driver.elf').stat().st_size
 row['system_commit']=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
 row['system_tree']=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD^{tree}'],text=True).strip()
 row['working_tree_dirty']=bool(subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain','--untracked-files=no'],text=True))
 row['source_sha256']['Services/text_input/SceneKeyboardV1.h']=hashlib.sha256((ROOT/'Services/text_input/SceneKeyboardV1.h').read_bytes()).hexdigest()
 (out/'build.json').write_text(json.dumps(row,indent=2)+'\n');rows[name]=row
validator=a.output/'validate-elf'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
for name in ('scene','text'):subprocess.run([str(validator),str(a.output/name/'driver.elf')],check=True)
(a.output/'hosts-build-record.json').write_text(json.dumps(rows,indent=2)+'\n')
