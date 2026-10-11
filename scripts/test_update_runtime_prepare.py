#!/usr/bin/env python3
"""Read-only Runtime source integration: admission only, no ELF execution."""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/update-prepare';out.mkdir(parents=True,exist_ok=True);rt=a.runtime
sources=['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp']
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*[ '-I'+str(rt/x) for x in ['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test/drivers/stubs']],*[str(rt/x) for x in sources],str(r/'test/native_apps/update_runtime_prepare.cpp'),'-ldl','-o',str(out/'prepare')],check=True)
for name in ('ota_update','app_store'):
 stage=out/name;stage.mkdir(exist_ok=True)
 manifest=json.loads((a.artifacts/name/'manifest.json').read_text());(stage/'app.json').write_text(json.dumps(manifest))
 drivers=[];grants=[]
 for i,req in enumerate(manifest['requires']):
  cap=req['capability'];grants.append({**req,'instance_id':6 if cap=='storage.key-value' else 0})
  if cap=='storage.key-value':grants.append({**req,'instance_id':1});continue
  driver={'type':'driver','id':'fixture-'+str(i),'version':'1.0.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':'fixture.elf','requires':[],'provides':[req]}
  file='provider-'+str(i)+'.json';(stage/file).write_text(json.dumps(driver));drivers.append({'manifest':file})
 (stage/'board.json').write_text(json.dumps({'schema':'riscrte.board-hardware','schema_version':1,'board_id':'admission-fixture','revision':'test','buses':[],'devices':[]}))
 (stage/'boot.json').write_text(json.dumps({'board':'board.json','default_app':name+'.elf','drivers':drivers,'app_capabilities':[{'manifest':'app.json','grants':grants}]}))
 subprocess.run([str(out/'prepare'),str(stage)],check=True)
