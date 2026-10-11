#!/usr/bin/env python3
"""Build native OTA/App Store telemetry receipts and compare unselected Watch bytes."""
import argparse,hashlib,io,json,os,subprocess,tarfile,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE='7b92c3904cf0822f378b61ca6ce1a07b2ca9460c'
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--compiler',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/telemetry-system/updates');p.add_argument('--skip-legacy',action='store_true');a=p.parse_args();out=a.output_dir.resolve();env=dict(os.environ,NATIVE_APP_CC=str(a.compiler.resolve()))
def run(repo,flags,destination,log):
 destination.mkdir(parents=True,exist_ok=True);cmd=['python3',str(repo/'scripts/build_portable_updates.py'),*flags,'--output-dir',str(destination)];r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=240);log.write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(r.stdout+r.stderr)
 return cmd
flags=['--product','x4','--time-profile','x4-native-time','--native-time-runtime-repo',str(a.runtime.resolve()),'--tagged-alarm-utilities',str(a.utilities.resolve()),'--alarm-client','--quick-actions','--quick-radios','--paper-transitions','--stage-logs','--navigation','--display-rotation','90','--wifi-instance','15','--home-app','default.elf','--ble-broadcast'];out.mkdir(parents=True,exist_ok=True)
record={'source_commit':subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip(),'baseline_source':BASE,'hardware_verified':False,'command':run(ROOT,flags,out,out/'build.log'),'targets':{},'watch':{}}
for name in ['ota_update','app_store']:
 target=out/name;m=json.loads((target/(name+'.json')).read_text());r=json.loads((target/'build-record.json').read_text());receipt=json.loads((target/'x4-native-app.json').read_text());digest=hashlib.sha256((target/(name+'.elf')).read_bytes()).hexdigest()
 assert m['version']==r['version']==receipt['version']=='1.2.2' and digest==r['sha256']==receipt['elf_sha256']
 assert len(r['required_grants'])==12 and r['required_grants']==receipt['required_grants']
 assert {'capability':'telemetry.broadcast','api':1,'instance_id':0} in receipt['required_grants']
 assert receipt['build_defines']==r['build_defines'] and '-DPORTABLE_BLE_BROADCAST' in receipt['build_defines'] and '-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF' in receipt['build_defines']
 assert receipt['ble_broadcast']['source_sha256']==r['ble_broadcast']['source_sha256'] and receipt['sdk_sha256']=={name:r['native_time_sdk']['sha256'].get(name,r['tagged_alarm_sdk']['sha256'].get(name)) for name in receipt['sdk_sha256']}
 assert not r['update_policy']['feed_configured'] and r['update_policy']['product']=='xteink-x4-pro'
 record['targets'][name]={'version':'1.2.2','sha256':digest,'bytes':r['size_bytes'],'grants':12,'clean_source':not r['working_tree_dirty']};print(name+' native telemetry12 grants/receipt PASS',flush=True)
if not a.skip_legacy:
 with tempfile.TemporaryDirectory(prefix='broadcast-updates-flag-off-') as temporary:
  baseline=Path(temporary)/'source';baseline.mkdir();blob=subprocess.check_output(['git','-C',ROOT,'archive',BASE])
  with tarfile.open(fileobj=io.BytesIO(blob)) as tar:tar.extractall(baseline,filter='data')
  subprocess.run(['git','-C',baseline,'init','-q'],check=True);subprocess.run(['git','-C',baseline,'add','.'],check=True);subprocess.run(['git','-C',baseline,'-c','user.name=Fixture','-c','user.email=fixture@localhost','commit','-qm','Immutable update baseline'],check=True)
  flags=['--nova-ui','--alarm-client','--quick-actions','--quick-radios','--home-app','default.elf','--wifi-instance','15','--rtc-utc-offset-seconds','28800'];destinations=[]
  for name,repo in [('baseline',baseline),('current',ROOT)]:
   target=out/('watch-'+name);destinations.append(target);run(repo,flags,target,out/('watch-'+name+'.log'))
  for name in ['ota_update','app_store','software-update-firmware','software-update-apps']:
   elf=name+'.elf' if name in ('ota_update','app_store') else 'driver.elf'
   for filename in [elf,'manifest.json']:assert (destinations[0]/name/filename).read_bytes()==(destinations[1]/name/filename).read_bytes(),(name,filename)
   record['watch'][name]={'exact_bytes':True,'sha256':hashlib.sha256((destinations[1]/name/elf).read_bytes()).hexdigest()};print(name+' Watch flag-off identity PASS',flush=True)
(out/'builder-qualification.json').write_text(json.dumps(record,indent=2)+'\n')
