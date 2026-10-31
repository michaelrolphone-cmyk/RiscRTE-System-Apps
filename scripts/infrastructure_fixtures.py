#!/usr/bin/env python3
"""Generate closed schemas and synthetic, secret-free provisioning fixtures."""
import hashlib,json,pathlib,argparse
ROOT=pathlib.Path(__file__).resolve().parents[1]
def encoded(v):return json.dumps(v,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()
def digest(v):return hashlib.sha256(encoded(v)).hexdigest()
def obj(properties,required=None):return {'type':'object','properties':properties,'required':list(properties) if required is None else required,'additionalProperties':False}
def string(n=128,minimum=1):return {'type':'string','minLength':minimum,'maxLength':n}
def integer(lo=0,hi=9007199254740991):return {'type':'integer','minimum':lo,'maximum':hi}
def arr(item,n=8):return {'type':'array','items':item,'maxItems':n}
def write(directory,name,value):directory.mkdir(parents=True,exist_ok=True);(directory/name).write_text(json.dumps(value,indent=2)+'\n')
def generate(output,package_digest):
 caps=[('scheduler.drive','maintain'),('service.reconcile','maintain'),('service.health.observe','observe'),('source.poll','poll')]
 contracts=[];providers=[]
 for cap,operation in caps:
  maintenance=operation=='maintain';input=obj({'binding_id':string(),'binding_revision':integer(1)})
  properties={'target_epoch':string(),'progress':obj({'items_examined':integer(0,32),'items_advanced':integer(0,32)}),'more_work':{'type':'boolean'},'coalescible':{'const':True}} if maintenance else {'binding_id':string(),'binding_revision':integer(1),'healthy':{'type':'boolean'},'unchanged':{'type':'boolean'},'http_status':integer(0,599),'observed_at_ms':integer(),'vantage_point':string(),'coverage':{'const':'latest_state'},'lossless':{'const':False}}
  out=obj(properties);out['additionalProperties']=True
  evidence={'type':'object','required':['observer','boot_id','observed_at_ms','monotonic_ms'],'properties':{'observer':string(),'boot_id':string(64),'observed_at_ms':integer(),'monotonic_ms':integer(),'quiescent':{'const':True},'executor_identity':string(64)},'additionalProperties':False}
  c={'capability_id':cap,'api_version':1,'operations':{operation:{'input_schema':input,'output_schema':out,'evidence_schema':evidence,'effect_semantics':'target_deduplicated' if maintenance else 'repeatable_compute','execution_modes':['desired_state','job'] if maintenance else ['job']}}}
  ref={'capability_id':cap,'api_version':1,'contract_digest':digest(c)};contracts.append({'contract':c,'reference':ref})
  p={'provider_id':'esp32s3-infra-'+cap.replace('.','-'),'revision':8,'implementation_ref':{'kind':'risc-elf','profile':'esp32s3-infrastructure','package_digest':package_digest},'provides':[ref],'requires':[],'resource_bindings':[{'name':'tls','amount':1,'lifetime':'invocation'},{'name':'job','amount':1,'lifetime':'invocation'},{'name':'pending','amount':2,'lifetime':'invocation'}],'recovery_support':{'checkpoint_formats':['infra.source/1']},'cost_estimate':1,'cost_model':{'network_transactions':1,'maximum_ms':12000 if cap=='scheduler.drive' else 5000},'cost_evidence':'software_bounds_physical_qualification_pending'}
  providers.append({'provider':p,'reference':{'capability_id':cap,'provider_id':p['provider_id'],'revision':8,'definition_digest':digest(p),'package_digest':package_digest}})
 bindings=[];endpoints=[]
 for index,(cap,op) in enumerate(caps):
  bid=['scheduler-main','capabilities-main','service-health','source-main'][index];maintenance=index<2
  b={'binding_id':bid,'revision':1,'capability':cap,'api_version':1,'operation':op,'endpoint_policy_id':bid+'-endpoint','credential_ref':bid+'-credential','cadence_ms':15000 if maintenance else 60000,'timeout_ms':5000,'request_max_bytes':16384,'response_max_bytes':4096 if maintenance else 32768,'effect_class':'target_deduplicated' if maintenance else 'repeatable_compute','max_items':4,'max_elapsed_ms':1000,'continuation':'target-cursor' if maintenance else 'accepted-checkpoint','expires_at_ms':1893456000000,'authorization_scope':[cap+'.'+op],'required':maintenance}
  if index==0:b.update(revision=2,max_items=1,max_elapsed_ms=8000,timeout_ms=12000,cadence_ms=60000)
  if maintenance:b['wake_semantics']='coalescible-durable-work'
  else:b.update(format='json',collection_bound=32,projection=['status','version'],expected_status_min=200,expected_status_max=299,latest_state=True)
  bindings.append(b);endpoints.append({'endpoint_policy_id':b['endpoint_policy_id'],'url':'https://'+['scheduler.example','capabilities.example','health.example','source.example'][index]+('/v1/maintenance/call' if maintenance else '/status'),'method':'POST' if maintenance else 'GET','capability':cap,'credential_ref':b['credential_ref'],'site_credential_ref':'','authorization_scope':b['authorization_scope'],'maintenance_qualified':maintenance})
 configuration={'schema_version':1,'revision':1,'bindings':bindings,'enabled_providers':[c for c,_ in caps],'retry':{'initial_ms':1000,'maximum_ms':300000,'full_jitter':True},'telemetry':{'interval_ms':60000,'diagnostic_bytes':16384},'update_policy':{'mode':'serial-owner'}}
 policy={'schema_version':1,'wifi':{'ssid':'EXAMPLE-NETWORK','credential_ref':'wifi'},'service':{'url':'https://capabilities.example/v1/call','credential_ref':'participant','site_credential_ref':'','time_server':'pool.ntp.org','contact_interval_ms':30000,'contracts':[x['reference'] for x in contracts],'providers':[x['reference'] for x in providers]},'endpoints':endpoints,'configuration':configuration,'low_water':{'internal_free':49152,'internal_largest':16384,'dma_free':16384,'dma_largest':8192,'psram_free':1572864,'psram_largest':262144,'stack_headroom':4096},'provenance':{'spec_revision':'f0e52f9fb704bd984a1b7ed8d0bdc9c74b2596a875d58ea74b7354f2c6667ac2','fixture':'synthetic-no-credentials'}}
 state={'schema_version':1,'installation_id':'example-installation','credential_generation':1,'credentials':{},'participant_id':'','cursor':0,'pending':[],'job':None,'checkpoints':{},'configuration_revision':1}
 write(output,'node-policy.example.json',policy);write(output,'node-state.example.json',state);write(output,'catalog.json',{'contracts':contracts,'providers':providers})
 # Every safety field is declared. Optional capability-specific fields are
 # checked again by the firmware's tighter semantic validator.
 binding_properties={k:( {'type':'boolean'} if isinstance(v,bool) else integer(1) if isinstance(v,int) else arr(string(),8) if isinstance(v,list) else string(256)) for k,v in bindings[0].items()}
 for k in ['format','semantic_field','semantic_value']:binding_properties[k]=string(4096)
 binding_properties.update(collection_bound=integer(1,128),projection=arr(string(64),16),expected_status_min=integer(100,599),expected_status_max=integer(100,599),latest_state={'type':'boolean'})
 binding_properties['api_version']={'const':1};binding_properties['capability']={'enum':[c for c,_ in caps]};binding_properties['max_items']=integer(1,32);binding_properties['max_elapsed_ms']=integer(100,10000);binding_properties['timeout_ms']=integer(100,30000);binding_properties['request_max_bytes']=integer(1,16384);binding_properties['response_max_bytes']=integer(1,32768);binding_properties['cadence_ms']=integer(1000,86400000)
 required=[k for k in bindings[0] if k!='wake_semantics'];bs=obj(binding_properties,required)
 cs=obj({'schema_version':{'const':1},'revision':integer(1),'bindings':arr(bs),'enabled_providers':arr({'enum':[c for c,_ in caps]},4),'retry':obj({'initial_ms':{'const':1000},'maximum_ms':{'const':300000},'full_jitter':{'const':True}}),'telemetry':obj({'interval_ms':integer(15000,86400000),'diagnostic_bytes':{'const':16384}}),'update_policy':obj({'mode':{'const':'serial-owner'}})})
 digest_schema={'type':'string','pattern':'^[a-f0-9]{64}$'}
 contract_ref=obj({'capability_id':{'enum':[c for c,_ in caps]},'api_version':{'const':1},'contract_digest':digest_schema})
 provider_ref=obj({'capability_id':{'enum':[c for c,_ in caps]},'provider_id':string(),'revision':integer(1),'definition_digest':digest_schema,'package_digest':digest_schema})
 ep=obj({'endpoint_policy_id':string(),'url':{'type':'string','pattern':'^https://','maxLength':2048},'method':{'enum':['GET','POST']},'capability':{'enum':[c for c,_ in caps]},'credential_ref':string(128,0),'site_credential_ref':string(128,0),'authorization_scope':arr(string()),'maintenance_qualified':{'type':'boolean'}})
 ps=obj({'schema_version':{'const':1},'wifi':obj({'ssid':string(32),'credential_ref':string()}),'service':obj({'url':{'type':'string','pattern':'^https://','maxLength':2048},'credential_ref':string(),'site_credential_ref':string(128,0),'time_server':string(253),'contact_interval_ms':integer(15000,30000),'contracts':arr(contract_ref,4),'providers':arr(provider_ref,4)}),'endpoints':arr(ep),'configuration':cs,'low_water':obj({k:integer(1,16777216) for k in policy['low_water']}),'provenance':obj({'spec_revision':digest_schema,'fixture':string()})})
 relay_filter=obj({'path':string(),'op':{'enum':['eq','prefix']},'value':string(256,0)})
 relay_subscription=obj({'id':string(64),'channel':{'enum':['capabilities.events','bpmn.events','scheduler.events','rhizome.events']},'match':obj({'types':arr(string(),8),'filters':arr(relay_filter,4)}),'url':{'type':'string','pattern':'^https://','maxLength':2048},'auth':obj({'header':string(64),'credential_ref':string()}),'signing_credential_ref':string(),'enabled':{'type':'boolean'}})
 relay_subscription['properties'].update(channels=arr({'enum':['capabilities.events','bpmn.events','scheduler.events','rhizome.events']},4),destination=obj({'kind':{'enum':['webhook','mcp_events']},'origin':string(256),'event_name':string(128)},['kind','origin']),principal=string(200),generation=integer(1),expires_at=integer(0),revoked={'type':'boolean'},fanout_group=string(128),invocation=obj({'routine_id':string(128),'routine_version':string(128),'assignment_ref':string(200,0),'deadline':integer(0),'effect_key':string(200,0)}),previous_signing_credential_ref=string(128),rotation_until=integer(0))
 relay_subscription['required']=['id','match','url','enabled']
 relay_subscription['anyOf']=[{'required':['channel']},{'required':['channels']}]
 ps['properties']['relay']=obj({'schema_version':{'const':1},'relay_id':string(),'url':{'type':'string','pattern':'^https://','maxLength':2048},'credential_ref':string(),'site_credential_ref':string(128,0),'allowed_origins':arr(string(256),8),'webhook_credential_refs':arr(string(),24),'subscriptions':arr(relay_subscription,8)},['schema_version','relay_id','url','credential_ref','site_credential_ref'])
 write(output,'relay-subscription.schema.json',{'$schema':'https://json-schema.org/draft/2020-12/schema',**relay_subscription})
 write(output,'node-policy.schema.json',{'$schema':'https://json-schema.org/draft/2020-12/schema',**ps});write(output,'configuration.schema.json',{'$schema':'https://json-schema.org/draft/2020-12/schema',**cs})
 common={'version':'1.0.0','architecture':'xtensa-esp32s3'}
 station={'type':'driver','id':'infra-station',**common,'driver_abi':2,'file_name':'infra-station.elf','requires':[{'capability':'platform.radio','api':1},{'capability':'hardware.device','api':1}],'provides':[{'capability':'infra.station','api':1}],'hardware_compatibility':[{'compatible':'espressif,esp32s3-wifi','revisions':['unspecified'],'config_type':'radio.integrated','config_version':1}]}
 node={'type':'driver','id':'infra-node',**common,'driver_abi':2,'file_name':'infra-node.elf','requires':[{'capability':c,'api':v} for c,v in [('platform.http-client',2),('platform.clock',1),('platform.entropy',1),('storage.app-data.bound',1),('infra.station',1),('platform.tcp-listener',1)]],'provides':[{'capability':'infra.node','api':1}]}
 app={'type':'application','id':'infra-agent',**common,'file_name':'default.elf','entry':'app_main','requires':[{'capability':'infra.node','api':1},{'capability':'memory.heap','api':1}]}
 board={'schema':'riscrte.board-hardware','schema_version':1,'board_id':'esp32s3-infrastructure','revision':'16mb-qio-octal-psram','buses':[],'devices':[{'instance_id':1,'chip':{'vendor':'espressif','model':'esp32s3','revision':'unspecified'},'compatible':'espressif,esp32s3-wifi','config_type':'radio.integrated','config_version':1,'config':{'unit':0,'features':3}}]}
 boot={'board':'board.json','default_app':'default.elf','provider_activation':'demand','drivers':[{'manifest':'infra-station.json','instance_id':1},{'manifest':'infra-node.json','bindings':[{'capability':'infra.station','api':1,'instance_id':1}],'app_data':[{'name':'node-policy.json','namespace':100,'access':'read'},{'name':'node-state.bin','namespace':100,'access':'read-write'}]}],'app_capabilities':[{'manifest':'app.json','grants':[{'capability':'infra.node','api':1,'instance_id':0},{'capability':'memory.heap','api':1,'instance_id':0}]}]}
 for name,value in [('infra-station.json',station),('infra-node.json',node),('app.json',app),('board.json',board),('boot.json',boot)]:write(output/'bootfs',name,value)
 return policy,state
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,default=ROOT/'profiles/esp32s3-infrastructure/fixtures');p.add_argument('--package-digest',default='0'*64);a=p.parse_args();generate(a.output,a.package_digest)
