#!/usr/bin/env python3
"""Verify exact selected Watch alarm source from the two public bounded bundles."""
import argparse,hashlib,json,tempfile
from pathlib import Path
from verify_watch19_system_custody import git,require,read_custody as read_base,PREREQUISITE as PUBLIC_PREREQUISITE
ROOT=Path(__file__).resolve().parents[1]
CUSTODY=ROOT/'docs/source-checkpoints/watch-alarm-terminal'
SOURCE='e2b2b8b7d903acb6aec6b07ebee7d037de2babb1'
TREE='4d69b22ddfc2c8582e14808bb0ec88e6ef1fea6e'
BASE='1b1c2009935a5489c897b12c12bd4a37e684779f'
PUBLIC='93fa422b82c174c2c56ad5875f63a6bcbee97b63'
PUBLIC_TREE='dc07c265d21f17c2b8c4e3ce42e0a7e49164310b'
PUBLIC_PARENT='fbdfad1f8db4e332528387713b85cbcc15232be1'
DIGEST='df39f03d134178b57baf4cafbb8f471cd1025f4d7e7c3ab852f33f4f78290bcc'
SIZE=11274
BASE_CUSTODY_FILES={'.github/workflows/watch19-source-custody.yml','docs/source-checkpoints/watch19-system/README.md',
 'docs/source-checkpoints/watch19-system/System.bundle','docs/source-checkpoints/watch19-system/manifest.json',
 'scripts/verify_watch19_system_custody.py','tests/test_watch19_system_custody.py'}
def read_custody(directory=CUSTODY):
 manifest=json.loads((directory/'manifest.json').read_text())
 expected={'schema':1,'repository':'michaelrolphone-cmyk/RiscRTE-System-Apps','bundle':'System.bundle','bundle_bytes':SIZE,
  'bundle_sha256':DIGEST,'prerequisite_commit':BASE,'bundle_ref':'HEAD','source_commit':SOURCE,'source_tree':TREE,
  'public_code_commit':PUBLIC,'public_code_tree':PUBLIC_TREE,'public_code_parent':PUBLIC_PARENT}
 require(manifest==expected,'Custody manifest differs from fixed source mapping')
 bundle=directory/'System.bundle';payload=bundle.read_bytes()
 require(len(payload)==SIZE,'Bundle byte count mismatch')
 require(hashlib.sha256(payload).hexdigest()==DIGEST,'Bundle SHA-256 mismatch')
 header,separator,_=payload.partition(b'\n\n')
 expected_header=f'# v2 git bundle\n-{BASE} Recover Watch18 System source tree from verified source inputs\n{SOURCE} HEAD'.encode()
 require(separator and header==expected_header,'Bundle prerequisite/ref mismatch')
 return manifest,bundle
def verify(repository=ROOT):
 manifest,bundle=read_custody();_,base_bundle=read_base();repository=repository.resolve()
 require(git(repository,'rev-parse',f'{PUBLIC}^{{tree}}').stdout.strip()==PUBLIC_TREE,'Public tree mismatch')
 require(git(repository,'rev-parse',f'{PUBLIC}^').stdout.strip()==PUBLIC_PARENT,'Public parent mismatch')
 with tempfile.TemporaryDirectory(prefix='watch-alarm-source-') as temporary:
  fresh=Path(temporary);git(fresh,'init','--bare','--quiet')
  git(fresh,'fetch','--quiet','--no-tags',str(repository),f'{PUBLIC_PREREQUISITE}:refs/heads/prerequisite')
  require(git(fresh,'cat-file','-e',f'{SOURCE}^{{commit}}',check=False).returncode!=0,'Source unexpectedly present before import')
  git(fresh,'fetch','--quiet','--no-tags',str(base_bundle),'HEAD:refs/heads/watch-base')
  require(git(fresh,'rev-parse','watch-base').stdout.strip()==BASE,'Imported prerequisite mismatch')
  git(fresh,'bundle','verify',str(bundle));git(fresh,'fetch','--quiet','--no-tags',str(bundle),'HEAD:refs/heads/watch-alarm')
  require(git(fresh,'rev-parse','watch-alarm').stdout.strip()==SOURCE,'Imported source mismatch')
  require(git(fresh,'rev-parse',f'{SOURCE}^{{tree}}').stdout.strip()==TREE,'Imported tree mismatch')
  require(git(fresh,'rev-parse',f'{SOURCE}^').stdout.strip()==BASE,'Imported source parent mismatch')
  git(fresh,'fetch','--quiet','--no-tags',str(repository),f'{PUBLIC}:refs/heads/public-code')
  changes=git(fresh,'diff','--name-status',SOURCE,PUBLIC).stdout.splitlines()
  require(set(changes)=={'A\t'+path for path in BASE_CUSTODY_FILES},'Public code differs beyond existing base custody files')
  git(fresh,'fsck','--full','--strict')
 return dict(manifest,verified=True,fresh_repository_import=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--repository',type=Path,default=ROOT);a=p.parse_args()
 print(json.dumps(verify(a.repository),indent=2))
