#!/usr/bin/env python3
"""Admit actual Wi-Fi manifests with an unchanged production Runtime parser.

Synthetic providers/board are metadata fixtures only; no ELF is loaded. They
are not product grants. The product integration owner selects real bindings.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(runtime, manifests):
 out=ROOT/'build/wifi-manifest';out.mkdir(parents=True,exist_ok=True)
 sources=['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp',
          'src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp']
 includes=['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test/drivers/stubs']
 binary=out/'admit'
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
                 *['-I'+str(runtime/p) for p in includes],*[str(runtime/p) for p in sources],
                 str(ROOT/'test/native_apps/wifi_manifest_admission.cpp'),'-o',str(binary)],check=True)
 records=[]
 for at,path in enumerate(manifests):
  fixture=out/f'fixture-{at}';fixture.mkdir(exist_ok=True)
  manifest=json.loads(path.read_text());(fixture/'wifi_settings.json').write_bytes(path.read_bytes())
  drivers=[];devices=[];grants=[]
  for index,req in enumerate(manifest['requires']):
   cap=req['capability']
   if cap=='storage.key-value':
    grants.extend(dict(req,instance_id=ns) for ns in [6,1]);continue
   instance=15 if cap=='net.wifi' else 7 if cap=='board.battery' else 101+index
   name=f'fixture-{index}'
   devices.append({'instance_id':instance,'chip':{'vendor':'test','model':'gpio','revision':'unspecified'},
    'compatible':'test,gpio','config_type':'gpio.bank','config_version':1,
    'config':{'pins':[index+1],'active_high':True,'pull_up':False,'debounce_us':0,'long_press_us':0,'click_min_us':0}})
   driver={'type':'driver','id':name,'version':'1.0.0','driver_abi':2,'architecture':'xtensa-esp32s3',
    'file_name':name+'.elf','requires':[{'capability':'hardware.device','api':1}], 'provides':[req],
    'hardware_compatibility':[{'compatible':'test,gpio','revisions':['unspecified'],'config_type':'gpio.bank','config_version':1}]}
   (fixture/(name+'.json')).write_text(json.dumps(driver))
   drivers.append({'manifest':name+'.json','instance_id':instance});grants.append(dict(req,instance_id=instance))
  board={'schema':'riscrte.board-hardware','schema_version':1,'board_id':'wifi-parser-fixture','revision':'unspecified','buses':[],'devices':devices}
  boot={'board':'board.json','default_app':'wifi_settings.elf','drivers':drivers,'app_capabilities':[{'manifest':'wifi_settings.json','grants':grants}]}
  (fixture/'board.json').write_text(json.dumps(board));(fixture/'boot.json').write_text(json.dumps(boot))
  subprocess.run([str(binary),str(fixture)],check=True)
  # Regression for the integration owner's reported unknown-field mismatch.
  manifest['profile']='must-be-rejected';(fixture/'wifi_settings.json').write_text(json.dumps(manifest))
  invalid=subprocess.run([str(binary),str(fixture)],capture_output=True,text=True)
  assert invalid.returncode==1 and 'invalid app identity/declarations' in invalid.stderr
  (fixture/'wifi_settings.json').write_bytes(path.read_bytes())
  records.append({'manifest':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'admitted':True,'unknown_profile_rejected':True})
 record={'runtime_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip(),
         'runtime_source_sha256':{p:hashlib.sha256((runtime/p).read_bytes()).hexdigest() for p in sources},
         'scope':'metadata admission only; synthetic providers; no execution or product grants','manifests':records}
 (out/'admission-record.json').write_text(json.dumps(record,indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('manifests',type=Path,nargs='+');a=p.parse_args();run(a.runtime.resolve(),[p.resolve() for p in a.manifests])
