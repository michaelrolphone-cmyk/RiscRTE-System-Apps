#!/usr/bin/env python3
"""Generate immutable, credential-free static-product policy and contracts."""
import pathlib,json,hashlib,zipfile,os
ROOT=pathlib.Path(__file__).resolve().parents[1]
def digest(v):return hashlib.sha256(json.dumps(v,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def generate():
 p=json.loads((ROOT/'profiles/esp32s3-infrastructure/fixtures/node-policy.example.json').read_text())
 # A deterministic source package, separate from the generated policy, avoids
 # a self-referential firmware digest while pinning all executable inputs.
 runtime=pathlib.Path(os.environ.get('RISC_RUNTIME_ROOT',ROOT/'../relay-runtime')).resolve()
 outdir=pathlib.Path(os.environ.get('RELAY6CH_OUTPUT',ROOT/'../relay-delivery'));outdir.mkdir(parents=True,exist_ok=True)
 package=outdir/'riscrte-relay6ch-v1.0.0-source.zip'
 with zipfile.ZipFile(package,'w',zipfile.ZIP_STORED) as z:
  sources=list((ROOT/'Services/infra_node').glob('*'))+list((ROOT/'firmware/waveshare-relay6ch').rglob('*'))+[ROOT/'scripts/generate_relay6ch.py']
  sources += list((ROOT/'lib/EnrollmentCrypto').glob('*'))+list((ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2').glob('*'))
  sources += list((runtime/'sdk').rglob('*.h'))+list((runtime/'src/ports/esp32s3').glob('*.h'))+[runtime/'src/bootstrap/TcpListenerBackend.h',runtime/'src/diagnostics/StageLog.h',runtime/'scripts/native_idf_crt_bundle.py']
  for f in sorted(set(sources)):
   if not f.is_file() or f.name=='Factory.h':continue
   name=('runtime/'+f.relative_to(runtime).as_posix()) if f.is_relative_to(runtime) else ('apps/'+f.relative_to(ROOT).as_posix())
   info=zipfile.ZipInfo(name,date_time=(2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_STORED;info.external_attr=0o100644<<16;z.writestr(info,f.read_bytes())
 package_digest=hashlib.sha256(package.read_bytes()).hexdigest()
 base=json.loads((ROOT/'profiles/esp32s3-infrastructure/fixtures/catalog.json').read_text())
 p['service']['contracts']=[];p['service']['providers']=[]
 contracts=base['contracts'];providers=[]
 for entry in base['providers']:
  definition=entry['provider'];cap=entry['reference']['capability_id'];definition['provider_id']='waveshare-relay6ch-'+cap.replace('.','-');definition['revision']=1
  definition['implementation_ref']={'kind':'risc-native-static','profile':'waveshare-esp32-s3-relay-6ch','package_digest':package_digest}
  pin={'capability_id':cap,'provider_id':definition['provider_id'],'revision':1,'definition_digest':digest(definition),'package_digest':package_digest}
  providers.append({'provider':definition,'reference':pin});p['service']['providers'].append(pin)
 p['service']['contracts']=[c['reference'] for c in contracts]
 p['memory_profile']='internal-only';p['low_water']={'internal_free':49152,'internal_largest':16384,'dma_free':8192,'dma_largest':4096,'psram_free':0,'psram_largest':0,'stack_headroom':4096}
 p['wifi']['ssid']='';p['configuration']['bindings']=[];p['configuration']['enabled_providers']=[];p['endpoints']=[];p['allocation']={'automatic':True,'max_assignments':5}
 integer=lambda a,b:{'type':'integer','minimum':a,'maximum':b}
 entries=[('relay.bank','set',{'channel':integer(1,6),'on':{'type':'boolean'}},'convergent_state'),('serial.rs485','exchange',{'baud':integer(1200,115200),'parity':{'enum':['none','even','odd']},'stop_bits':integer(1,2),'tx_hex':{'type':'string','minLength':2,'maxLength':512,'pattern':'^[0-9a-f]*$'},'rx_max':integer(0,256),'timeout_ms':integer(10,2000)},'nonrepeatable_or_unobservable'),('sound.buzzer','tone',{'frequency_hz':integer(100,10000),'duration_ms':integer(1,5000)},'observable_effect'),('light.rgb','set',{'red':integer(0,255),'green':integer(0,255),'blue':integer(0,255)},'convergent_state')]
 for cap,op,fields,effect in entries:
  contract={'capability_id':cap,'api_version':1,'operations':{op:{'input_schema':{'type':'object','properties':fields,'required':list(fields),'additionalProperties':False},'output_schema':{'type':'object','properties':{'status':{'enum':['succeeded','unknown']}},'required':['status'],'additionalProperties':True},'evidence_schema':{'type':'object'},'effect_semantics':effect,'execution_modes':['job']}}}
  ref={'capability_id':cap,'api_version':1,'contract_digest':digest(contract)}
  provider={'provider_id':'waveshare-relay6ch-'+cap.replace('.','-'),'revision':1,'activation_policy':'independent_device_hardware/1','implementation_ref':{'kind':'risc-native-static','profile':'waveshare-esp32-s3-relay-6ch','package_digest':package_digest},'provides':[ref],'requires':[],'resource_bindings':[{'name':'job','amount':1,'lifetime':'invocation'},{'name':'pending','amount':2,'lifetime':'invocation'}],'recovery_support':{'checkpoint_formats':[]},'cost_estimate':1,'cost_model':{'maximum_ms':8000},'cost_evidence':'software_bounds_physical_qualification_pending'}
  pin={'capability_id':cap,'provider_id':provider['provider_id'],'revision':1,'definition_digest':digest(provider),'package_digest':package_digest}
  p['service']['contracts'].append(ref);p['service']['providers'].append(pin);contracts.append({'contract':contract,'reference':ref});providers.append({'provider':provider,'reference':pin})
 # Empty Wi-Fi deliberately enters the shipped setup portal. Keep the regular
 # validator's SSID bound using a sentinel; no secret or endpoint authority.
 p['wifi']['ssid']='SETUP-REQUIRED'
 out=ROOT/'firmware/waveshare-relay6ch/main/Factory.h'
 out.write_text('#pragma once\nstatic const char factory_policy[]=R"POLICY('+json.dumps(p,separators=(',',':'))+')POLICY";\n')
 dest=ROOT/'profiles/waveshare-esp32-s3-relay-6ch';dest.mkdir(exist_ok=True)
 (dest/'catalog.json').write_text(json.dumps({'contracts':contracts,'providers':providers},indent=2)+'\n')
 (dest/'node-policy.example.json').write_text(json.dumps(p,indent=2)+'\n')
if __name__=='__main__':generate()
