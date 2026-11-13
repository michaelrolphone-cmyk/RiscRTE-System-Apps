#!/usr/bin/env python3
"""Measure loader-equivalent legacy ELF metadata compaction without changing a cohort."""
import argparse,hashlib,json,subprocess,sys,tempfile
from pathlib import Path
from compose_shared_app_cohort import tree
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('cohort','watch','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--compiler',required=True);a=p.parse_args()
 if a.output.exists():raise ValueError('Fresh measurement output required')
 a.output.mkdir(parents=True)
 sys.path.insert(0,str((a.watch/'scripts').resolve()))
 from compact_current_elf import compact
 # Existing completed shared application artifacts and Home are immutable.
 excluded={'default.elf','file_browser.elf','lists.elf','model_viewer.elf','contexts.elf'}
 records={}
 for lane in ('watch','wave','x4'):
  rows={};entries=tree(a.cohort/lane/'after')
  for name,data in sorted(entries.items()):
   if not name.endswith('.elf') or Path(name).name in excluded:continue
   with tempfile.TemporaryDirectory(prefix='legacy-elf-measure-') as tmp:
    target=Path(tmp)/'candidate.elf';target.write_bytes(data)
    result=compact(target,a.compiler,debug_path=Path(tmp)/'original.elf')
    repeat=Path(tmp)/'repeat.elf';repeat.write_bytes(data)
    compact(repeat,a.compiler,debug_path=Path(tmp)/'repeat-original.elf')
    if target.read_bytes()!=repeat.read_bytes():raise ValueError('Compaction repeat differs '+name)
    rows[name]=result
  records[lane]={'files':rows,'saving_bytes':sum(r['before_bytes']-r['after_bytes'] for r in rows.values()),
                 'input_sha256':{n:hashlib.sha256(b).hexdigest() for n,b in entries.items()}}
  print('LEGACY METADATA MEASUREMENT '+lane+' '+json.dumps({'saving_bytes':records[lane]['saving_bytes'],'files':rows}),flush=True)
 (a.output/'measurement.json').write_text(json.dumps({'schema':1,'applied':False,'excluded':sorted(excluded),'products':records},sort_keys=True,indent=2)+'\n')
if __name__=='__main__':main()
