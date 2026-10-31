#!/usr/bin/env python3
"""Explicit paired repair: back up first; preserve enrollment keys, credentials and pending custody."""
import argparse,copy,hashlib,json,os,pathlib
from install_infrastructure import Serial,verify,update_plan,DATA
from inspect_infrastructure_state import read_snapshot,summary
from provision_infrastructure import seed
ROOT=pathlib.Path(__file__).resolve().parents[1]
def migrate(policy,state,target):
 new_policy,new_state=copy.deepcopy(policy),copy.deepcopy(state)
 for document in [new_policy,*([new_state['setup_policy']] if 'setup_policy' in new_state else [])]:
  if document['service']['contracts']!=target['service']['contracts']:raise ValueError('Contract migration requires explicit review')
  old=document['service']['providers'];new=target['service']['providers']
  if [(p['provider_id'],p['capability_id']) for p in old]!=[(p['provider_id'],p['capability_id']) for p in new]:raise ValueError('Provider cohort mismatch')
  if old!=new and any((p['revision'],p['package_digest']) not in [(6,'e8e06f34552ecf0df59b3b871310ccf7c842e5e67b3ede755462d065c3c330ac'),(7,'5d9e0a0bdf42eea15b57aa6eb1842cf2cd75f67d7daf20899af406f3c1372d93')] for p in old):raise ValueError('Repair accepts only the published v1.4.1/v1.4.2 cohort')
  document['service']['providers']=copy.deepcopy(new)
 # Only implementation pins change. In particular, preserve the key that opens
 # an enrollment request already sealed to this device, and every pending entry.
 for key in state:
  if key!='setup_policy' and new_state[key]!=state[key]:raise ValueError('Unrelated state changed')
 return new_policy,new_state
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bundle',type=pathlib.Path,default=ROOT);p.add_argument('--port',required=True);p.add_argument('--backup',type=pathlib.Path,required=True);p.add_argument('--baud',type=int,default=460800);a=p.parse_args();bundle=a.bundle.resolve();verify(bundle)
 a.backup.mkdir(parents=True,exist_ok=False);os.chmod(a.backup,0o700);serial=Serial(a.port,a.baud);(a.backup/'preflight.txt').write_text(serial.check());before=a.backup/'flash-before.bin';print(serial.call('read-flash','0','0x1000000',before));os.chmod(before,0o600);flash=before.read_bytes()
 writes=update_plan(bundle,flash);policy,state,sequence=read_snapshot(bundle,flash);report=summary(state,sequence);print(json.dumps(report,indent=2));(a.backup/'pending-identities.json').write_text(json.dumps(report,indent=2)+'\n')
 target=json.loads((bundle/'fixtures/node-policy.example.json').read_text());policy,state=migrate(policy,state,target)
 original=a.backup/'appdata-before.bin';original.write_bytes(flash[DATA[0]:DATA[0]+DATA[1]]);os.chmod(original,0o600);migrated=a.backup/'appdata-migrated.bin';seed(bundle,policy,state,migrated,original,sequence=sequence+1)
 check=bytearray(flash);check[DATA[0]:DATA[0]+DATA[1]]=migrated.read_bytes();actual_policy,actual_state,actual_sequence=read_snapshot(bundle,check)
 if (actual_policy,actual_state,actual_sequence)!=(policy,state,sequence+1):raise ValueError('Migration readback mismatch')
 # Verify the new pair, then the preserved data, and activate the pair last.
 writes.insert(len(writes)-1,(DATA[0],migrated.read_bytes()))
 for i,(offset,data) in enumerate(writes):
  path=a.backup/f'repair-write-{i}.bin';path.write_bytes(data);os.chmod(path,0o600);serial.write(offset,path)
 print('Paired repair verified. Enrollment identity and pending requests preserved. Reset the board; keep the private backup.')
if __name__=='__main__':main()
