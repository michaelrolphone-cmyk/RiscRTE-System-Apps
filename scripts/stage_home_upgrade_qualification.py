#!/usr/bin/env python3
"""Retain exact owning hosted upgrade proofs; do not manufacture qualification."""
import argparse,hashlib,json,os,subprocess,zipfile
from pathlib import Path
RUNTIME='712ecc3f894dba99d9c9638941778a82f5856b97'
BASELINE='0052d1fc42e9cfc82d626c5a247d460919c87935'
MARKERS={
 'native-upgrade.log':['Production native bank adapter: markers PASS','Production native bank adapter: restart PASS','Production native bank adapter: restart-unknown PASS','New Runtime boots unchanged raw store with stale cohort runtime metadata, then admits encoded default through actual cohort API; appdata/NVS preserved PASS'],
 'paired-upgrade.log':['Two-step native-first raw clone then cohort: reverse bank direction, cuts and rollback preserve last confirmed pair PASS']}
def digest(data):return hashlib.sha256(data).hexdigest()
def api(path):return json.loads(subprocess.check_output(['gh','api','repos/michaelrolphone-cmyk/RiscRTE/'+path]))
def verify_archive(raw):
 with zipfile.ZipFile(__import__('io').BytesIO(raw)) as z:
  names=z.namelist()
  if len(names)!=len(set(names)):raise ValueError('Duplicate proof archive members')
  for name in names:
   p=Path(name)
   if p.is_absolute() or '..' in p.parts:raise ValueError('Unsafe proof archive member')
  for suffix,markers in MARKERS.items():
   found=[n for n in names if n==suffix or n.endswith('/'+suffix)]
   if len(found)!=1:raise ValueError('Missing unique actual owning proof log')
   log=z.read(found[0]).decode()
   if any(marker not in log for marker in markers):raise ValueError('Actual owning assertion missing')
  boot=[n for n in names if n=='bootloader.bin' or n.endswith('/bootloader.bin')]
  if len(boot)!=1:raise ValueError('Missing exact bootloader')
  rawboot=z.read(boot[0])
  if len(rawboot)!=22640 or digest(rawboot)!='459d11a2d2cfd56e98edfc3ae4a9c05389ca2c0767c3ef69d1dfb16050f017db':raise ValueError('Bootloader proof differs')
def stage(lock,output):
 if output.exists() or set(lock)!= {'normal','sanitized'}:raise ValueError('Fresh complete proof staging required')
 prepared={}
 for mode,item in lock.items():
  run=api('actions/runs/'+str(item['run_id']));job=api('actions/jobs/'+str(item['job_id']))
  if run['head_sha']!=RUNTIME or run['conclusion']!='success' or job['run_id']!=item['run_id'] or job['conclusion']!='success':raise ValueError('Owning upgrade CI not qualified at exact source')
  artifact=api('actions/artifacts/'+str(item['artifact_id']))
  if artifact['workflow_run']['id']!=item['run_id'] or artifact['expired']:raise ValueError('Artifact not bound to owning run')
  raw=subprocess.check_output(['gh','api','repos/michaelrolphone-cmyk/RiscRTE/actions/artifacts/'+str(item['artifact_id'])+'/zip'])
  if digest(raw)!=item['sha256']:raise ValueError('Owning upgrade artifact bytes differ')
  verify_archive(raw);prepared[mode]=raw
 output.mkdir(parents=True)
 for mode,raw in prepared.items():(output/(mode+'.zip')).write_bytes(raw)
 receipt={'schema':1,'status':'owning-upgrade-proofs-verified','baseline_runtime':BASELINE,'decoder_runtime':RUNTIME,
  'upgrade_order':['firmware-only-current-raw-clone','reboot-health-confirm','decoder-admits-final-cohort','reboot-health-confirm'],
  'proofs':lock,'scope':'Hosted compositional actual NativeBank and paired-transaction proofs; paired health modeled; no continuous cross-version native reboot trace or device execution.'}
 (output/'receipt.json').write_text(json.dumps(receipt,sort_keys=True,indent=2)+'\n')
 print('Exact normal/sanitized owning upgrade proofs retained')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lock',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();stage(json.loads(a.lock.read_text()),a.output)
