#!/usr/bin/env python3
"""Read a private flash backup offline; print pending identities, never bodies or secrets."""
import argparse,json,os,pathlib,struct,subprocess,tempfile
from provision_infrastructure import compile_seed,unframe
ROOT=pathlib.Path(__file__).resolve().parents[1]
def read_records(bundle,flash):
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
  return policy,raw[offset:]
def read_snapshot(bundle,flash):
 policy,raw=read_records(bundle,flash);state,sequence=unframe(raw);return policy,state,sequence
def diagnose_frame(raw):
 """Report only structure, never values, keys, identities or credential bytes."""
 report={'frame_bytes':len(raw)}
 try:state,sequence=unframe(raw)
 except (ValueError,UnicodeError,json.JSONDecodeError,struct.error):
  return {**report,'status':'frame_or_json_invalid'}
 report.update(status='checksum_and_json_valid',journal_sequence=sequence)
 max_depth=max_active=max_object=max_array=max_string=0
 def visit(value,depth,ancestors):
  nonlocal max_depth,max_active,max_object,max_array,max_string
  if isinstance(value,dict):
   max_depth=max(max_depth,depth+1);max_object=max(max_object,len(value))
   for index,child in enumerate(value.values(),1):
    active=ancestors+index;max_active=max(max_active,active);visit(child,depth+1,active)
  elif isinstance(value,list):
   max_depth=max(max_depth,depth+1);max_array=max(max_array,len(value))
   for child in value:visit(child,depth+1,ancestors)
  elif isinstance(value,str):max_string=max(max_string,len(value.encode('utf-8')))
 visit(state,0,0)
 report.update(max_container_depth=max_depth,max_active_keys=max_active,max_object_members=max_object,max_array_items=max_array,max_string_bytes=max_string)
 report['legacy_shape_within_bounds']=max_depth<=16 and max_active<=64 and max_object<=64 and max_array<=128 and max_string<=4096
 report['repaired_shape_within_bounds']=max_depth<=24 and max_active<=256 and max_object<=64 and max_array<=128 and max_string<=4096
 report['scope']='Shape and checksum only; not full firmware schema validation or proof of physical corruption.'
 return report
def summary(state,sequence):
 return {'journal_sequence':sequence,'pending':[{'action':p.get('action'),'operation_id':p.get('request',{}).get('operation_id'),'family':p.get('request',{}).get('family'),'operation':p.get('request',{}).get('operation')} for p in state.get('pending',[])],'last_rejections':state.get('protocol_rejections',{}).get('items',[])}
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--bundle',type=pathlib.Path,default=ROOT);parser.add_argument('--flash',type=pathlib.Path,required=True);parser.add_argument('--diagnose',action='store_true',help='Print only redacted storage/shape diagnostics');args=parser.parse_args()
 if args.diagnose:
  try:policy,raw=read_records(args.bundle.resolve(),args.flash.read_bytes());report=diagnose_frame(raw)
  except subprocess.CalledProcessError as error:report={'status':'read_only_extraction_failed','reader_exit_code':error.returncode}
  print(json.dumps(report,indent=2))
 else:
  policy,state,sequence=read_snapshot(args.bundle.resolve(),args.flash.read_bytes());print(json.dumps(summary(state,sequence),indent=2))
