#!/usr/bin/env python3
"""Stage exact already-qualified public dependencies for owning cohort CI."""
import argparse,hashlib,json,shutil,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--lock',type=Path,default=ROOT/'docs/shared-home/published-dependencies.json')
 a=p.parse_args();lock=json.loads(a.lock.read_text())
 if a.output.exists():raise ValueError('Fresh dependency staging required')
 a.output.mkdir(parents=True);packages=a.output/'packages';packages.mkdir()
 for family,release in lock['releases'].items():
  root=a.output/'receipts'/family;root.mkdir(parents=True)
  for item in release['assets']:
   name=item['name']
   if Path(name).name!=name or not item['url'].startswith(release['repository']+'/releases/download/'+release['tag']+'/'):raise ValueError('Invalid locked release asset')
   with urllib.request.urlopen(item['url'],timeout=180) as response:raw=response.read()
   if len(raw)!=item['bytes'] or hashlib.sha256(raw).hexdigest()!=item['sha256']:raise ValueError('Published dependency differs '+name)
   (root/name).write_bytes(raw)
  if family=='scene':
   for profile in ('full','color-dynamic','watch-compact'):
    dest=packages/('scene-'+profile);dest.mkdir()
    for suffix,filename in [('.elf','driver.elf'),('.json','manifest.json'),('-build.json','build.json')]:
     shutil.copyfile(root/('scene-host-'+profile+suffix),dest/filename)
  elif family=='contexts':
   dest=packages/'contexts';dest.mkdir()
   for filename in ('contexts.elf','contexts.json','build.json'):shutil.copyfile(root/filename,dest/filename)
  else:raise ValueError('Unreviewed release family')
 (a.output/'verified-public-assets.json').write_text(json.dumps(lock,sort_keys=True,indent=2)+'\n')
 print('Published scene profiles and common Contexts: exact final public bytes verified')
if __name__=='__main__':main()
