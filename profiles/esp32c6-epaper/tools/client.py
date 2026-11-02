#!/usr/bin/env python3
"""Public consumer/issuer for the existing Capabilities Service wire protocol.
Credentials are read from a private JSON file, never printed or put in URLs.
Exact mutations and replies are durably journaled; unknown outcomes retain IDs.
"""
import argparse,hashlib,json,os,pathlib,secrets,sqlite3,time,urllib.request,urllib.error,uuid
from catalog import CONTRACT,PROVIDER,INPUT,PIN,canonical
ORIGIN='https://capabilities-service.cantikash.chatgpt.site'
class Client:
 def __init__(self,credentials,journal):
  self.credentials=credentials
  self.db=sqlite3.connect(journal)
  os.chmod(journal,0o600)
  self.db.execute('CREATE TABLE IF NOT EXISTS rpc(id TEXT PRIMARY KEY, request TEXT, response TEXT)')
 def request(self,path,data=None,method='POST',media='application/json'):
  if not path.startswith('/') or path.startswith('//'):raise ValueError('Same-service relative path required')
  headers={'Content-Type':media,'OAI-Sites-Authorization':'Bearer '+self.credentials['site_access']}
  if self.credentials.get('credential'):headers['Authorization']='Bearer '+self.credentials['credential']
  class NoRedirect(urllib.request.HTTPRedirectHandler):
   def redirect_request(self,*a,**kw):raise RuntimeError('Redirect refused')
  req=urllib.request.Request(ORIGIN+path,data=data,method=method,headers=headers)
  try:
   with urllib.request.build_opener(NoRedirect()).open(req,timeout=30) as response:return response.read()
  except urllib.error.HTTPError as e:
   raw=e.read()
   if e.code<500 and e.code!=429:return raw
   raise
 def call(self,family,operation,args,revision=None,key=None):
  key=key or str(uuid.uuid4());q={'protocol_version':'1.0','family':family,'operation':operation,'operation_id':key,'arguments':args}
  if revision is not None:q['expected_revision']=revision
  wire=canonical(q);old=self.db.execute('SELECT request,response FROM rpc WHERE id=?',(key,)).fetchone()
  if old:
   if wire!=old[0]:raise RuntimeError('Journal identity conflict')
   if old[1]:return self.require(json.loads(old[1]))
  else:
   with self.db:self.db.execute('INSERT INTO rpc VALUES(?,?,NULL)',(key,wire))
  r=json.loads(self.request('/v1/call',wire.encode()))
  if r.get('protocol_version')!='1.0' or r.get('mutation_outcome')=='unknown':raise RuntimeError('Unknown response: retry same command and journal')
  with self.db:self.db.execute('UPDATE rpc SET response=? WHERE id=?',(canonical(r),key))
  return self.require(r)
 @staticmethod
 def require(r):
  if r.get('status')!='completed':raise RuntimeError(f"Service {r.get('status')}: {r.get('code')}")
  return r['data']
 def upload(self,frame,participant,key):
  digest=hashlib.sha256(frame).hexdigest()
  a=self.call('artifact','upload_begin',{'sha256':digest,'size_bytes':len(frame),'media_type':'application/vnd.riscrte.epaper.mono1','scope':{'participant_ids':[participant]}},key=key+':begin')
  status=self.call('artifact','upload_status',{'artifact_id':a['id']})
  if status['state']!='verified':
   self.request(a['upload_path'],frame,'PUT','application/vnd.riscrte.epaper.mono1')
   self.call('artifact','upload_finalize',{'artifact_id':a['id'],'upload_identity':a['upload_identity']},key=key+':finalize')
  return {'artifact_id':a['id'],'size_bytes':len(frame),'sha256':digest}
def read_frame(path):
 data=pathlib.Path(path).read_bytes()
 if len(data)!=48000:raise ValueError('Exactly 48000 raw packed bytes required; use pack.py for an image')
 return data
def send(c,participant,offer,path,key,expected,expires):
 frame=read_frame(path);o=c.call('catalog','get',{'kind':'offer','entity_id':offer})
 if not o.get('ready') or o['participant_id']!=participant or o['provider_id']!='riscrte.esp32c6.epaper':raise ValueError('Current matching ready display offer required')
 artifact=c.upload(frame,participant,key)
 state={'format':'mono1-msb-black1','width':800,'height':480,'stride_bytes':100,'artifact':artifact}
 result=c.call('desired','put',{'desired_id':participant+':epaper-frame','logical_target':participant+':epaper','slot':'frame','schema':INPUT,'state':state,'provider_offer_id':offer,'delivery_semantics':'convergent','expires_at':expires,'arbitration_policy':{'kind':'single_publisher'}},revision=expected,key=key+':desired')
 return {'desired_id':result['id'],'desired_revision':result['desired_revision'],'sha256':artifact['sha256'],'status':'accepted; wait for actual.status=rendered'}
def main():
 p=argparse.ArgumentParser();p.add_argument('--credentials',required=True);p.add_argument('--journal',default='epaper-client.sqlite3');s=p.add_subparsers(dest='command',required=True)
 s.add_parser('publish')
 d=s.add_parser('demand');d.add_argument('--participant',required=True);d.add_argument('--expires-at',required=True,type=int);d.add_argument('--expected-revision',type=int,default=0)
 e=s.add_parser('enroll');e.add_argument('--out',required=True)
 r=s.add_parser('render');r.add_argument('frame');r.add_argument('--participant',required=True);r.add_argument('--offer',required=True);r.add_argument('--request-id',required=True);r.add_argument('--expected-revision',required=True,type=int);r.add_argument('--expires-at',required=True,type=int)
 q=s.add_parser('status');q.add_argument('desired_id')
 a=p.parse_args();credentials=json.loads(pathlib.Path(a.credentials).read_text());c=Client(credentials,a.journal)
 if a.command=='publish':
  c.call('catalog','contract_publish',{'contract':CONTRACT},key='epaper-contract-v1');out=c.call('catalog','provider_publish',{'provider':PROVIDER},key='epaper-c6-provider-v1')
 elif a.command=='enroll':
  # Saved before issuance so retries never lose the bootstrap credential.
  dest=pathlib.Path(a.out)
  if dest.exists():config=json.loads(dest.read_text())
  else:
   config={'bootstrap':secrets.token_urlsafe(32),'participant_credential':secrets.token_urlsafe(32),'site_access':credentials['site_access'],'ssid':'','password':'','request_id':str(uuid.uuid4()),'expires_at':int(time.time()*1000)+86400000}
   fd=os.open(dest,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
   with os.fdopen(fd,'w') as f:json.dump(config,f,indent=2);f.flush();os.fsync(f.fileno())
  out=c.call('participant','enrollment_issue',{'policy':{'bootstrap_sha256':hashlib.sha256(config['bootstrap'].encode()).hexdigest(),'accept_unpinned_work':False},'expires_at':config['expires_at']},key=config['request_id'])
  config['enrollment_id']=out['id'];dest.write_text(json.dumps(config,indent=2)+'\n');out={'config_file':str(dest),'expires_at':config['expires_at']}
 elif a.command=='demand':
  # The recovered project has one display. This demand activates one matching panel.
  out=c.call('catalog','capability_demand_put',{'demand':{'demand_id':a.participant+':epaper','capabilities':[PIN],'services':[],'required_labels':{'display':'uc8179-800x480-bw'},'count':1,'priority':100,'expires_at':a.expires_at}},revision=a.expected_revision)
 elif a.command=='render':out=send(c,a.participant,a.offer,a.frame,a.request_id,a.expected_revision,a.expires_at)
 else:out=c.call('desired','get',{'desired_id':a.desired_id})
 print(json.dumps(out,indent=2))
if __name__=='__main__':main()
