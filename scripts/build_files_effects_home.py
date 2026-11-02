#!/usr/bin/env python3
"""Run the owning Home recipe with exact archive-safe source pins and explicit overlay provenance."""
import argparse,hashlib,json,runpy,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for k in ('home','system','utilities','pins','output','cc'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--serial-transfer-mode',action='store_true');a=p.parse_args()
pins=json.loads(a.pins.read_text());roots={k:getattr(a,k).resolve() for k in ('home','system','utilities')}
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def verify():
 for role,record in pins['sources'].items():
  root=roots[role]
  for n,h in record['files'].items():
   rel=Path(n);assert not rel.is_absolute() and '..' not in rel.parts
   f=root/rel;assert f.is_file() and not f.is_symlink() and sha(f)==h,(role,n)
 assert sha(a.cc)==pins['compiler_sha256'],'Compiler executable differs'
verify();sys.path.insert(0,str(roots['home']/'scripts'));import build_inputs
original_manifest=build_inputs.file_manifest
def checked(root,expected,paths):
 role=next(k for k,v in roots.items() if v==Path(root).resolve());record=pins['sources'][role]
 assert expected==record['base_commit']
 actual=original_manifest(root,paths)
 expected_files={n:h for n,h in record['files'].items() if any(n==x or n.startswith(x+'/') for x in paths)}
 assert actual==expected_files,'Dependency paths differ from exact source pins'
 return {'base_commit':expected,'overlay':record['overlay'],'files_sha256':actual,'verification':'exact source bytes; archive-safe; no claim of unchanged Git revision'}
build_inputs.verify_dependency=checked
sys.argv=[str(roots['home']/'scripts/build.py'),'--system',str(roots['system']),'--utilities',str(roots['utilities']),'--output',str(a.output),'--cc',str(a.cc),'--role','default','--automatic-provision']+(['--serial-transfer-mode'] if a.serial_transfer_mode else [])
runpy.run_path(str(roots['home']/'scripts/build.py'),run_name='__main__');verify()
m=a.output/'default.json';manifest=json.loads(m.read_text());assert manifest['version']==('0.26.2' if a.serial_transfer_mode else '0.26.1'),'Owning Home package version differs'
receipt=a.output/'build.json';b=json.loads(receipt.read_text());b['files_effects_source_pins_sha256']=sha(a.pins);b['owning_recipe_sha256']=sha(roots['home']/'scripts/build.py');b['package_version']=manifest['version'];b['package_version_provenance']='Owning Home recipe version; wrapper asserts without mutation';receipt.write_text(json.dumps(b,indent=2)+'\n')
