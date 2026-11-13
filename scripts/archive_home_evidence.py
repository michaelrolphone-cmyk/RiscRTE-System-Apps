#!/usr/bin/env python3
"""Archive actual source/packing/admission records without manufacturing results."""
import argparse,json,zipfile
from pathlib import Path
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('incoming','closure','system'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args()
 def archive(output,groups):
  if output.exists():raise ValueError('Fresh evidence archive required')
  rows={}
  for label,root,select in groups:
   paths=[root] if root.is_file() else sorted(root.rglob('*'))
   for path in paths:
    if path.is_symlink():raise ValueError('Evidence symlink forbidden')
    if not path.is_file() or not select(path):continue
    relative=path.name if root.is_file() else path.relative_to(root).as_posix()
    key=label+'/'+relative
    if key in rows:raise ValueError('Duplicate evidence member')
    rows[key]=path.read_bytes()
  if not rows:raise ValueError('Empty evidence archive')
  with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
   for name,raw in sorted(rows.items()):
    info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
    info.external_attr=0o100644<<16;z.writestr(info,raw)
 def records(p):return p.suffix in ('.json','.log','.txt','.head')
 yes=lambda p:True
 archive(a.closure/'qualification.zip',[
  ('complete-home',a.incoming/'home',lambda p:records(p) and 'source' not in p.parts),
  ('clients',a.incoming/'clients',records),('owning',a.closure/'qualification',records),
  ('admission',a.closure/'admission',records),
  ('dependencies',a.closure/'dependencies',lambda p:records(p) and p.name not in ('manifest.json','contexts.json')),
 ])
 archive(a.closure/'provenance.zip',[
  ('reviewed-plan',a.closure/'plan.json',yes),
  ('assembly',a.closure/'assembly/assembly.json',yes),
  ('compaction',a.closure/'packages/compaction.json',yes),
  ('cohort-inputs',a.incoming/'cohort',lambda p:records(p) and 'composed' not in p.parts and 'inputs' not in p.parts),
  ('source-locks',a.closure/'source',lambda p:p.suffix=='.head'),
  ('reviewed-baselines',a.system/'docs/shared-home',records),
 ])
 print('Actual qualification and exact provenance records archived')
if __name__=='__main__':main()
