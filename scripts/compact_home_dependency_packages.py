#!/usr/bin/env python3
"""Compact only non-loader metadata with the owning semantic-equivalence gate.
Keep every original ELF sidecar and exact transformation receipt. Actual final
cohort Runtime admission remains required; this is not a live memory-fit test.
"""
import argparse,hashlib,json,shutil,subprocess,sys,tempfile
from pathlib import Path
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('packages','watch','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--compiler',required=True);a=p.parse_args()
 if a.output.exists():raise ValueError('Fresh compact package output required')
 watch=a.watch.resolve();sys.path.insert(0,str(watch/'scripts'))
 from compact_current_elf import compact
 subprocess.run(['git','-C',str(watch),'diff','--exit-code','HEAD','--'],check=True)
 source=subprocess.check_output(['git','-C',str(watch),'rev-parse','HEAD'],text=True).strip()
 recipe=hashlib.sha256((watch/'scripts/compact_current_elf.py').read_bytes()).hexdigest()
 shutil.copytree(a.packages,a.output)
 records={}
 for path in sorted(a.output.rglob('*.elf')):
  if 'debug' in path.relative_to(a.output).parts:continue
  original=path.read_bytes();relative=path.relative_to(a.output).as_posix()
  record=compact(path,a.compiler,debug_path=a.output/'debug'/relative)
  with tempfile.TemporaryDirectory(prefix='home-package-repeat-') as temp:
   repeat=Path(temp)/path.name;repeat.write_bytes(original)
   again=compact(repeat,a.compiler)
   if path.read_bytes()!=repeat.read_bytes():raise ValueError('Repeated compaction differs '+relative)
  records[relative]=record
 (a.output/'compaction.json').write_text(json.dumps({'schema':1,'owner':'michaelrolphone-cmyk/RiscRTE-Watch','source':source,'recipe_sha256':recipe,'artifacts':records,'target_execution':False},sort_keys=True,indent=2)+'\n')
 print(json.dumps({name:{k:v for k,v in row.items() if k in ('before_bytes','after_bytes','after_sha256')} for name,row in records.items()},sort_keys=True,indent=2))
if __name__=='__main__':main()
