#!/usr/bin/env python3
"""Build an explicitly pinned resident development cohort; never stage an install image.

All 21 delivered apps stay in the inventory. The accepted GameBoy binary is
recorded from delivered custody metadata only and is never opened or built.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
SPEC=ROOT/'examples/x4-resident-cohort.json'
RECORDS={'default':'build-evidence.json','springboard':'springboard-build-record.json','settings':'settings-build-record.json',
 'file_browser':'file_browser-build-record.json','wifi_settings':'wifi_settings-build-record.json',
 'ota_update':'build-record.json','app_store':'build-record.json','usb_sd_transfer':'usb_sd_transfer-build-record.json'}

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
def write(path,value):Path(path).write_text(json.dumps(value,indent=2)+'\n')
def require(ok,detail):
 if not ok:raise ValueError(detail)

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('system','runtime','utilities','productivity','product','sdk','baseline','compiler','output'):
  p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--msc-sdk',type=Path,help='Canonical delivered USB device MSC header directory for the System client cohort')
 p.add_argument('--spec',type=Path,default=SPEC,help='Exact cohort pin/feature specification')
 a=p.parse_args();spec_path=a.spec.resolve();spec=json.loads(spec_path.read_text())
 features=spec.get('features',{})
 for key in ('system','runtime','utilities','productivity','product'):
  path=getattr(a,key).resolve();setattr(a,key,path)
  require(git(path,'rev-parse','HEAD')==spec['pins'][key],key+' exact source pin differs')
  require(not git(path,'status','--porcelain','--untracked-files=no'),key+' tracked tree is dirty')
 a.output=a.output.resolve();a.baseline=a.baseline.resolve();a.sdk=a.sdk.resolve();a.compiler=a.compiler.resolve()
 require(not a.output.is_relative_to(a.baseline) and not a.baseline.is_relative_to(a.output),'Output overlaps delivered baseline')
 require(a.output!=a.system and not a.output.is_relative_to(a.system),'Use a separate output directory')
 if features.get('system_clients'):
  require(a.msc_sdk is not None,'The System client cohort requires --msc-sdk')
  a.msc_sdk=a.msc_sdk.resolve()
  require(sha(a.msc_sdk/'RiscUsbDeviceMscV1.h')==spec['usb_msc_sdk_sha256'],'Delivered USB SDK differs')
 version=subprocess.check_output([a.compiler,'--version'],text=True).splitlines()[0]
 require('8.4.0' in version and '2021r2-patch5' in version,'Pinned Xtensa GCC 8.4.0 2021r2-patch5 required')
 for name,digest in spec['display_sdk_sha256'].items():require(sha(a.sdk/name)==digest,'SDK input differs: '+name)
 store=a.baseline/'store';custody_path=a.baseline/'build-custody.json'
 require(sha(custody_path)==spec['baseline']['custody_sha256'],'Delivered custody receipt differs')
 require(sha(store/'boot.json')==spec['baseline']['boot_sha256'],'Delivered boot policy differs')
 custody=json.loads(custody_path.read_text());boot=json.loads((store/'boot.json').read_text())
 apps={}
 for path in sorted(store.glob('*.json')):
  manifest=json.loads(path.read_text())
  if manifest.get('type')=='application':apps[manifest['file_name']]=(manifest,path)
 require(set(apps)=={x['file_name'] for x in spec['inventory']},'Delivered application inventory differs')
 require({x['manifest'] for x in boot['app_capabilities']}=={path.name for _,path in apps.values()},'Admission inventory differs')
 for item in spec['inventory']:
  manifest,path=apps[item['file_name']]
  require(manifest['version']==item['delivered_version'] and sha(path)==item['manifest_sha256'],'Delivered manifest differs: '+path.name)
  require(custody['store_files'][item['file_name']]==item['delivered_binary'],'Delivered binary receipt differs: '+item['file_name'])
 catalog=json.loads((ROOT/spec['catalog']).read_text())
 require({x['file_name'] for x in catalog['apps']}==set(apps)-{'default.elf','springboard.elf'},'Springboard catalog loses an installed app')
 a.output.mkdir(parents=True,exist_ok=True);write(a.output/'delivered-inventory.json',spec['inventory'])
 catalog_path=a.output/'springboard-catalog.json';write(catalog_path,catalog)
 common=['--resident-shell-client','--resident-runtime-sdk',str(a.runtime/'sdk/app'),
  '--alarm-client','--home-app','default.elf','--tagged-alarm-utilities',str(a.utilities),
  '--native-time-runtime-repo',str(a.runtime),'--touch-scrolling','--ble-broadcast']
 commands={
  'default':[sys.executable,str(a.system/'scripts/build_paper_clock.py'),'--desk-clock','--sparse-start',
   '--desk-lock-home','--desk-points-face','--navigation','--alarm-client','--wake-light-restore',
   '--local-sleep-source',str(a.product/'minimal/apps/portable_sleep.c'),'--sleep-capability','x4.power',
   '--sleep-sdk',str(a.sdk),'--retained-wake-sdk',str(a.runtime/'sdk/app'),
   '--tagged-alarm-utilities',str(a.utilities),'--paper-crossfade','--paper-display-sdk',str(a.sdk),
   '--paper-transitions','--quick-actions','--quick-radios','--quick-usb-transfer',
   '--resident-shell-host','--resident-runtime-sdk',str(a.runtime/'sdk/app'),'--ble-broadcast',
   '--x4-idle-source',str(a.product/'minimal/apps/portable_idle_sleep.c'),'--x4-idle-sdk',str(a.sdk),
   '--x4-idle-runtime-sdk',str(a.runtime/'sdk/driver')],
  'springboard':[sys.executable,str(a.system/'scripts/build_portable_springboard.py'),*common,
   '--time-profile','x4-native-time','--catalog',str(catalog_path),'--return-app','default.elf',
   '--paper-crossfade','--paper-display-sdk',str(a.sdk)],
  'settings':[sys.executable,str(a.system/'scripts/build_portable_settings.py'),*common,
   '--settings-profile','x4-native-time','--settings-list-scrolling','--home-desk-lock','--unpadded-hours','--return-app','springboard.elf']}
 if features.get('host_contexts_rf_only'):commands['default']+=['--contexts-rf-only']
 if features.get('host_app_loading'):commands['default']+=['--resident-loading-catalog',str(catalog_path)]
 if features.get('legacy_handoff'):commands['default']+=['--resident-legacy-handoff']
 if features.get('resident_policy'):
  for command in commands.values():command+=['--resident-policy']
 if features.get('system_clients'):
  selected=[*common,'--resident-policy','--navigation','--time-profile','x4-native-time','--stage-logs']
  commands['file_browser']=[sys.executable,str(a.system/'scripts/build_portable_file_browser.py'),*selected,
   '--storage-capability','storage.volume','--storage-instance','9','--file-handlers','--return-app','springboard.elf']
  commands['wifi_settings']=[sys.executable,str(a.system/'scripts/build_portable_wifi.py'),*selected,'--wifi-instance','15','--return-app','springboard.elf']
  commands['updates']=[sys.executable,str(a.system/'scripts/build_portable_updates.py'),*selected,'--product','x4','--wifi-instance','15','--apps-only']
  commands['usb_sd_transfer']=[sys.executable,str(a.system/'scripts/build_portable_usb_transfer.py'),
   '--resident-shell-client','--resident-runtime-sdk',str(a.runtime/'sdk/app'),'--resident-policy',
   '--tagged-alarm-utilities',str(a.utilities),'--msc-sdk',str(a.msc_sdk)]
 for name,command in commands.items():command.extend(['--output-dir',str(a.output/name)])
 write(a.output/'commands.json',{'environment':{'NATIVE_APP_CC':str(a.compiler)},'commands':commands})
 env={**os.environ,'NATIVE_APP_CC':str(a.compiler)};results={};policy={x['manifest']:x for x in boot['app_capabilities']}
 for name,command in commands.items():
  with (a.output/(name+'.log')).open('w') as log:
   subprocess.run(command,cwd=a.system,env=env,check=True,stdout=log,stderr=subprocess.STDOUT)
 directories={name:a.output/name for name in commands if name!='updates'}
 if 'updates' in commands:directories.update({name:a.output/'updates'/name for name in ('ota_update','app_store')})
 for name,directory in directories.items():
  elf=directory/(name+'.elf');manifest=json.loads(elf.with_suffix('.json').read_text())
  record=json.loads((directory/RECORDS[name]).read_text());resident=record['resident_shell']
  if name in spec.get('selected_versions',{}):
   require(manifest['version']==spec['selected_versions'][name],'Selected app reservation differs: '+name)
  require(resident['role']==('host' if name=='default' else 'foreground'),'Wrong compiled resident role')
  for header,digest in resident['sdk_sha256'].items():require(sha(a.runtime/'sdk/app'/header)==digest,'Resident SDK drift')
  symbols=subprocess.check_output([str(a.compiler).removesuffix('gcc')+'nm',str(elf)],text=True)
  definitions=[line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3 and line.split()[-2]!='U']
  quick=[s for s in definitions if s=='pqa_render']
  require(len(quick)==(1 if name=='default' else 0),'Duplicate or missing Quick Actions renderer')
  require(definitions.count('risc_resident_app_descriptor_v1')==1,'Missing resident descriptor')
  if name!='default':
   require(not any(s.startswith(('pqa_render','pqa_sheet','pqa_font','quick_ref_','quick_paper_render')) for s in definitions),'Foreground contains shared UI symbols')
   require(b'QUICK ACTIONS' not in elf.read_bytes(),'Foreground contains sheet title')
  expected={(x['capability'],x['api']) for x in manifest['requires']};old=policy[name+'.json']['grants']
  proposed=[g for g in old if (g['capability'],g['api']) in expected]
  for binding in spec.get('added_bindings',{}).get(name,[]):
   require(binding in policy['default.json']['grants'],'New binding is not admitted to the delivered host')
   require((binding['capability'],binding['api']) in expected and binding not in proposed,'Unexpected added binding')
   proposed.append(binding)
  require({(g['capability'],g['api']) for g in proposed}==expected,'New requirement has no delivered binding')
  result={'elf':str(elf),'bytes':elf.stat().st_size,'sha256':sha(elf),'version':manifest['version'],
   'delivered_version':apps[name+'.elf'][0]['version'],'role':resident['role'],'quick_render_definitions':len(quick),
   'record':str(directory/RECORDS[name]),'proposed_grants':proposed,
   'added_grants':[g for g in proposed if g not in old],
   'removed_grants':[g for g in old if g not in proposed],'resident_sdk_sha256':resident['sdk_sha256']}
  results[name]=result;print(name+': '+str(result['bytes'])+' bytes, Quick renderer='+str(len(quick))+', target checks PASS',flush=True)
 require(sum(r['quick_render_definitions'] for r in results.values())==1,'Cohort does not have exactly one renderer')
 spring=json.loads((a.output/'springboard'/RECORDS['springboard']).read_text())
 require(spring['catalog']['count']==19 and spring['touch_scrolling']['apps_per_page']==12,'Springboard inventory/grid changed')
 for name in ('Apps/springboard.c','Apps/springboard_paper.inc','Apps/springboard_scroll.inc','Apps/springboard_pages.h'):
  expected=subprocess.check_output(['git','-C',str(a.system),'show',spec['springboard_preservation_pin']+':'+name])
  require((a.system/name).read_bytes()==expected,'Selected .43 Springboard source changed: '+name)
 require('-DPORTABLE_SPRINGBOARD_TOUCH_SCROLL' in spring['build_defines'],'Selected scrolling branch is absent')
 selection={'api':1,'host':'default.elf','foreground':[name+'.elf' for name in results if name!='default']}
 if features.get('legacy_handoff'):
  selection['legacy']=[x['file_name'] for x in spec['inventory'] if Path(x['file_name']).stem not in results]
 write(a.output/'policy-proposal.json',{'installable':False,'baseline_policy_count':len(boot['app_capabilities']),
  'initial_development_selection':selection,
  'future_converted_foreground':[x['file_name'] for x in spec['inventory'] if x['file_name'] not in ('default.elf','gameboy.elf')],
  'rebuilt_app_grants':{name:result['proposed_grants'] for name,result in results.items()},
  'untouched_policies':[x for x in boot['app_capabilities'] if Path(x['manifest']).stem not in results],
  'blockers':spec['blockers']})
 write(a.output/'cohort-receipt.json',{'schema':1,'purpose':str(len(results))+'-app development build; not an installable product',
  'recipe_sha256':sha(__file__),'spec_sha256':sha(spec_path),'features':features,'pins':spec['pins'],'compiler':version,
  'baseline':spec['baseline'],'delivered_application_count':len(apps),'springboard_catalog_count':19,
  'built':results,'remaining': [x for x in spec['inventory'] if Path(x['file_name']).stem not in results],
  'quick_render_total':1,'springboard_preservation_pin':spec['springboard_preservation_pin'],
  'gameboy_handling':'delivered metadata only; no binary read/build/test/change',
  'blockers':spec['blockers'],'hardware_qualified':False})
 print('Selected cohort PASS; full delivered inventory retained in recipe; product installation blocked as recorded')

if __name__=='__main__':main()
