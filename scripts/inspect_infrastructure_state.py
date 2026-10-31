#!/usr/bin/env python3
"""Read a private flash backup offline; print pending identities, never bodies or secrets."""
import argparse,json,os,pathlib,struct,subprocess,tempfile
from provision_infrastructure import compile_seed,unframe
ROOT=pathlib.Path(__file__).resolve().parents[1]
def read_snapshot(bundle,flash):
 if len(flash)!=16777216:raise ValueError('Expected a complete 16 MiB private flash backup')
 with tempfile.TemporaryDirectory(prefix='infra-inspect-') as directory:
  root=pathlib.Path(directory);os.chmod(root,0o700)
  # compile_seed verifies every bundled LittleFS source against reviewed hashes.
  compile_seed(bundle,root/'seed',core=bundle/'tools/littlefs')
  core=bundle/'tools/littlefs';reader=root/'reader'
  subprocess.run(['cc','-std=c11','-O2','-DLFS_READONLY','-DLFS_NO_DEBUG','-DLFS_NO_WARN','-DLFS_NO_ERROR','-I'+str(core),str(bundle/'scripts/infrastructure_read.c'),str(core/'lfs.c'),str(core/'lfs_util.c'),'-o',str(reader)],check=True)
  image=root/'appdata.bin';image.write_bytes(flash[0x270000:0x2f0000]);os.chmod(image,0o600)
  raw=subprocess.check_output([str(reader),str(image)])
  length=struct.unpack_from('<I',raw)[0];policy=json.loads(raw[4:4+length]);offset=4+length;length=struct.unpack_from('<I',raw,offset)[0];offset+=4
  if offset+length!=len(raw):raise ValueError('Invalid extracted frame lengths')
  state,sequence=unframe(raw[offset:]);return policy,state,sequence
def summary(state,sequence):
 return {'journal_sequence':sequence,'pending':[{'action':p.get('action'),'operation_id':p.get('request',{}).get('operation_id'),'family':p.get('request',{}).get('family'),'operation':p.get('request',{}).get('operation')} for p in state.get('pending',[])],'last_rejections':state.get('protocol_rejections',{}).get('items',[])}
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--bundle',type=pathlib.Path,default=ROOT);parser.add_argument('--flash',type=pathlib.Path,required=True);args=parser.parse_args()
 policy,state,sequence=read_snapshot(args.bundle.resolve(),args.flash.read_bytes());print(json.dumps(summary(state,sequence),indent=2))
