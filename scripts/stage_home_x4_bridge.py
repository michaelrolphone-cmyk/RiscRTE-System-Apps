#!/usr/bin/env python3
"""Stage the exact qualified X4 singleton bridge closure for product admission."""
import argparse,hashlib,json,shutil,zipfile
from pathlib import Path,PurePosixPath
LOCK={'repository':'michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO','artifact_id':13144852409,'sha256':'e81c99637e22ffcfb57c80ed32e2c30d32d9d1e21f4c9168221d7dbae7dcfb1e','source':'8f2bd2bf0b1ce7a84084d6194d47c389b7441e61'}
PACKAGES={'x4pro_home_power_singleton':'x4-power','x4pro_home_display_power_singleton':'x4-display-power','x4pro_power':'x4-physical-power'}
def digest(data):return hashlib.sha256(data).hexdigest()
def stage(archive,output):
 if output.exists():raise ValueError('Fresh X4 staging required')
 with archive.open('rb') as f:
  if hashlib.file_digest(f,'sha256').hexdigest()!=LOCK['sha256']:raise ValueError('Qualified X4 artifact differs')
 with zipfile.ZipFile(archive) as z:
  names=z.namelist()
  if len(names)!=len(set(names)):raise ValueError('Duplicate archive entries')
  for name in names:
   p=PurePosixPath(name)
   if p.is_absolute() or '..' in p.parts or '\\' in name:raise ValueError('Unsafe archive path')
  def member(suffix):
   found=[n for n in names if n==suffix or n=='qualification/'+suffix]
   if len(found)!=1:raise ValueError('Exact owning archive member required '+suffix)
   return z.read(found[0])
  receipt_raw=member('singleton/receipt.json');receipt=json.loads(receipt_raw)
  if receipt['source']!=LOCK['source'] or receipt['physical_hardware_tested'] is not False:raise ValueError('Qualified source/scope differs')
  if set(p['id'] for p in receipt['products'])!=set(PACKAGES):raise ValueError('Incomplete target bridge products')
  if set(r['mode'] for r in receipt['runs'])!={'normal','sanitized'}:raise ValueError('Missing owning qualification mode')
  required={'enter-owner-deep','enter-owner-light','enter-owner-display','enter-owner-display-resume','enter-take-deep','enter-take-light','enter-take-display','enter-take-display-resume','enter-take-nested','enter-take-nested-unknown','enter-owner-display-unknown'}
  for mode in ('normal','sanitized'):
   if not required.issubset({r['case'] for r in receipt['runs'] if r['mode']==mode}):raise ValueError('Missing actual terminal custody regressions')
  staged={}
  for product in receipt['products']:
   name=product['id'];raw=member('singleton/target/'+name+'/driver.elf');manifest=member('singleton/target/'+name+'/manifest.json')
   if digest(raw)!=product['sha256'] or len(raw)!=product['bytes']:raise ValueError('Target artifact differs '+name)
   original='minimal/drivers/'+name+'/manifest.json'
   if digest(manifest)!=receipt['sha256'][original]:raise ValueError('Owning manifest differs '+name)
   staged[PACKAGES[name]]={'driver.elf':raw,'manifest.json':manifest}
  sources={name:member('source/'+name) for name in ('x4.tar.gz','x4.head','runtime.tar.gz','runtime.head','reader.tar.gz','reader.head','watch.tar.gz','watch.head','system.tar.gz','system.head','utilities.tar.gz','utilities.head')}
  if sources['x4.head'].decode().strip()!=LOCK['source']:raise ValueError('Archived source identity differs')
  output.mkdir(parents=True)
  for name,files in staged.items():
   dest=output/'packages'/name;dest.mkdir(parents=True)
   for filename,data in files.items():(dest/filename).write_bytes(data)
  (output/'source').mkdir()
  for name,data in sources.items():(output/'source'/name).write_bytes(data)
  (output/'receipt.json').write_bytes(receipt_raw)
  (output/'artifact-lock.json').write_text(json.dumps(LOCK,sort_keys=True,indent=2)+'\n')
  print('Exact X4 singleton/physical provider closure staged; actual product graph admission still required')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--archive',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 a=p.parse_args();stage(a.archive,a.output)
