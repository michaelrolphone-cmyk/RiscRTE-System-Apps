#!/usr/bin/env python3
"""Offline, private image provisioning. No device I/O or credential transmission."""
import argparse,getpass,hashlib,json,os,pathlib,struct,subprocess,tempfile,zlib,uuid
ROOT=pathlib.Path(__file__).resolve().parents[1]
def frame(value,sequence=1):
 data=json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode();h=b'INFRAS31'+struct.pack('<IIQI',1,len(data),sequence,zlib.crc32(data));return h+struct.pack('<I',zlib.crc32(h))+data
def unframe(data):
 if len(data)<32 or data[:8]!=b'INFRAS31':raise ValueError('Not an infrastructure snapshot')
 version,size,seq,crc,headercrc=struct.unpack('<IIQII',data[8:32])
 if version!=1 or size!=len(data)-32 or not seq or zlib.crc32(data[32:])!=crc or zlib.crc32(data[:28])!=headercrc:raise ValueError('Corrupt snapshot')
 return json.loads(data[32:]),seq
def compile_seed(runtime,out):
 core=ROOT/'tools/littlefs'
 if not core.exists():core=runtime.resolve().parent/'native-build/esp-idf/risc_littlefs/pinned/littlefs'
 pins={'lfs.c':'0b9845f350c33aa448a916847d2246b76c8e296eed8e7e296ae0430242cfda6c','lfs.h':'a8c8d70f0863fbbc46ce17c5dc7673b40f1b3c9e7e10f3bf33fd28f03dc67703','lfs_util.c':'f2fbde533670560434bd9f5a547174cc7c5a4670a02c47b4bd85180dced8b2ec','lfs_util.h':'03e912a6e9894c9d10c61f5da22b89ebe0bb778af67972d7b67a5f160731bf72'}
 for name,expected in pins.items():
  if hashlib.sha256((core/name).read_bytes()).hexdigest()!=expected:raise ValueError('LittleFS source pin mismatch: '+name)
 subprocess.run(['cc','-std=c11','-O2','-DLFS_NO_DEBUG','-DLFS_NO_WARN','-DLFS_NO_ERROR','-I'+str(core),str(ROOT/'scripts/infrastructure_seed.c'),str(core/'lfs.c'),str(core/'lfs_util.c'),'-o',str(out)],check=True)
 return out
def seed(runtime,policy,state,output,existing=None):
 if len(json.dumps(policy,separators=(',',':')).encode())>16384:raise ValueError('Policy exceeds16KiB')
 data=frame(state)
 if len(data)>49152:raise ValueError('State exceeds48KiB')
 output.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='infra-private-') as d:
  t=pathlib.Path(d);os.chmod(t,0o700);p=t/'policy.json';q=t/'state.bin';p.write_text(json.dumps(policy,separators=(',',':')));q.write_bytes(data);os.chmod(p,0o600);os.chmod(q,0o600)
  exe=compile_seed(runtime,t/'seed');subprocess.run([str(exe),str(output.resolve()),str(p),str(q),'update' if existing else 'create',*([str(existing.resolve())] if existing else [])],check=True)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',default=ROOT.parent/'runtime',type=pathlib.Path);p.add_argument('--policy',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);p.add_argument('--credentials',type=pathlib.Path,help='Private JSON mapping credential references to secret values');p.add_argument('--participant-id');p.add_argument('--enrollment-id');p.add_argument('--installation-id');p.add_argument('--state',type=pathlib.Path,help='Existing decoded JSON snapshot for an explicit offline repair; preserves pending custody');p.add_argument('--existing-image',type=pathlib.Path);p.add_argument('--template-only',action='store_true',help='Build an unprovisioned image which blocks before Wi-Fi or enrollment');a=p.parse_args()
 policy=json.loads(a.policy.read_text());state=json.loads(a.state.read_text()) if a.state else {'schema_version':1,'installation_id':a.installation_id or str(uuid.uuid4()),'credential_generation':1,'credentials':{},'participant_id':a.participant_id or '','cursor':0,'pending':[],'job':None,'checkpoints':{},'configuration_revision':policy['configuration']['revision']}
 if a.state and not a.existing_image:raise ValueError('Existing state requires its existing volume; never reset custody with a fresh image')
 if not a.template_only:
  credentials=json.loads(a.credentials.read_text()) if a.credentials else {}
  refs={policy['wifi']['credential_ref'],policy['service']['credential_ref'],*(e['credential_ref'] for e in policy['endpoints'] if e['credential_ref']),*(e['site_credential_ref'] for e in policy['endpoints'] if e['site_credential_ref'])}
  if policy['service']['site_credential_ref']:refs.add(policy['service']['site_credential_ref'])
  for ref in sorted(refs):
   if ref not in credentials:credentials[ref]=getpass.getpass('Secret for '+ref+': ')
   if not isinstance(credentials[ref],str) or not credentials[ref] or len(credentials[ref].encode())>1024 or any(ord(x)<32 for x in credentials[ref]):raise ValueError('Invalid secret for '+ref)
  state['credentials'].update(credentials)
  if not state['participant_id']:
   if not a.enrollment_id:raise ValueError('--enrollment-id required for a new participant')
   state['enrollment_id']=a.enrollment_id;state['bootstrap']=getpass.getpass('One-use enrollment bootstrap: ');state['credential_sha256']=hashlib.sha256(credentials[policy['service']['credential_ref']].encode()).hexdigest()
  if len(json.dumps(state['credentials']).encode())>4096:raise ValueError('Credential table exceeds4KiB')
 seed(a.runtime,policy,state,a.output,a.existing_image);print('Private app-data image created. Keep it off public repositories.')
if __name__=='__main__':main()
