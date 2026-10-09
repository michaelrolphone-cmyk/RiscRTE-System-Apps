#!/usr/bin/env python3
"""Verify the focused public compatibility source; no original bundle import."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE='16058588cd8e64021b8c89c4fbad3f2ecc736a5a'
CODE='ccd57a97d27316a15deb2ddd601f565c24bcdf96'
TREE='3e2300d416c0ca54b4f22ea1dd03bec126c980c8'
def git(root,*args):return subprocess.check_output(['git','-C',str(root),*args])
def verify(repository=ROOT):
 m=json.loads((ROOT/'docs/source-checkpoints/x4-alarm-terminal/input-comparison.json').read_text())
 assert m['public_base']==BASE and m['public_code']==CODE and m['public_code_tree']==TREE
 assert not m['whole_tree_equivalent'] and not m['original_source_uploaded']
 assert git(repository,'rev-parse',CODE+'^').decode().strip()==BASE
 assert git(repository,'rev-parse',CODE+'^{tree}').decode().strip()==TREE
 names=set(git(repository,'diff-tree','--no-commit-id','--name-only','-r',CODE).decode().splitlines())
 assert names=={row['path'] for row in m['files']} and len(names)==12
 for row in m['files']:
  assert hashlib.sha256(git(repository,'show',CODE+':'+row['path'])).hexdigest()==row['sha256'],row['path']
  assert row['equal_original']==(row['sha256']==row['original_sha256'])
 return {'verified_public_inputs':True,'public_code':CODE,'tree':TREE,'files':len(names),'original_source_uploaded':False}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--repository',type=Path,default=ROOT);a=p.parse_args()
 print(json.dumps(verify(a.repository),indent=2))
