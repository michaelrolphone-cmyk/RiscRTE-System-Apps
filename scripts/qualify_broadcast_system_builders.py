#!/usr/bin/env python3
"""Build selected System telemetry profiles and prove Watch flag-off byte identity."""
import argparse,hashlib,io,json,os,subprocess,tarfile,tempfile
from pathlib import Path
import portable_broadcast_build as broadcast
ROOT=Path(__file__).resolve().parents[1];BASE='99f9a74a7ae10df3c9fcb81872e7c0855cb2b42b'
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--display-sdk',type=Path,required=True);p.add_argument('--catalog',type=Path,required=True);p.add_argument('--compiler',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/telemetry-system');p.add_argument('--skip-legacy',action='store_true');a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
rt=a.runtime.resolve();u=a.utilities.resolve();display=a.display_sdk.resolve();env=dict(os.environ,NATIVE_APP_CC=str(a.compiler.resolve()))
common=['--alarm-client','--tagged-alarm-utilities',str(u),'--quick-actions','--paper-transitions','--stage-logs','--home-app','default.elf','--ble-broadcast']
recipes={
 'settings':['--settings-profile','x4-native-time','--performance-runtime-repo',str(rt),'--performance-display-sdk',str(display),'--touch-scrolling','--home-desk-lock','--alarm-settings','--quick-radios','--return-app','springboard.elf'],
 'springboard':['--time-profile','x4-native-time','--performance-runtime-repo',str(rt),'--performance-display-sdk',str(display),'--paper-crossfade','--paper-display-sdk',str(display),'--return-app','default.elf','--catalog',str(a.catalog.resolve())],
 'file_browser':['--time-profile','x4-native-time','--native-time-runtime-repo',str(rt),'--touch-scrolling','--storage-capability','storage.volume','--storage-instance','9','--file-handlers','--return-app','springboard.elf'],
 'wifi_settings':['--time-profile','x4-native-time','--native-time-runtime-repo',str(rt),'--touch-scrolling','--wifi-instance','15','--return-app','springboard.elf']}
builders={n:('wifi' if n=='wifi_settings' else n) for n in recipes};evidence={'baseline_source':subprocess.check_output(['git','-C',ROOT,'rev-parse',BASE],text=True).strip(),'source_commit':subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip(),'hardware_verified':False,'recipes':{},'targets':{},'watch':{}}
def run(repo,name,opts,destination,log):
 command=['python3',str(repo/'scripts'/('build_portable_'+builders[name]+'.py')),*opts,'--output-dir',str(destination)]
 result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=180);log.write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError(result.stdout+result.stderr)
 return command
for name,extra in recipes.items():
 target=out/name;command=run(ROOT,name,[*common,*extra],target,out/(name+'-build.log'));m=json.loads((target/(name+'.json')).read_text());r=json.loads((target/(name+'-build-record.json')).read_text());receipt=json.loads((target/'x4-native-app.json').read_text())
 assert m['version']==broadcast.VERSIONS[name]==receipt['version']==r['version']
 assert m['requires'].count(broadcast.CAPABILITY)==1 and receipt['requires']==m['requires']
 assert receipt['build_defines']==r['build_defines'] and set(broadcast.DEFINES)<=set(receipt['build_defines'])
 assert receipt['ble_broadcast']==r['ble_broadcast'] and receipt['required_grants']==r['required_grants'] and len(receipt['required_grants'])<=16
 assert receipt['elf_sha256']==hashlib.sha256((target/(name+'.elf')).read_bytes()).hexdigest()==r['sha256']
 assert all(hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest for path,digest in r['ble_broadcast']['source_sha256'].items())
 if name=='settings':assert r['home_desk_lock']['readonly'] and r['touch_scrolling'] and r['sleep_modes']==[]
 if name=='springboard':assert r['paper_transition']['enabled'] and 'RiscDisplayOutputSnapshotV1.h' in receipt['sdk_sha256']
 if name=='file_browser':assert r['touch_scrolling'] and r['storage_selection']['primary']=={'capability':'storage.volume','api':1,'instance_id':9}
 if name=='wifi_settings':assert r['touch_scrolling'] and r['wifi_instance']==15 and r['grant_bindings']['storage.key-value']==[6,1]
 evidence['recipes'][name]=command;evidence['targets'][name]={'version':m['version'],'sha256':r['sha256'],'size_bytes':r['size_bytes'],'grants':len(receipt['required_grants']),'clean_source':not r['working_tree_dirty']};print(name+' selected target/receipt PASS',flush=True)
if not a.skip_legacy:
 with tempfile.TemporaryDirectory(prefix='broadcast-flag-off-') as temporary:
  baseline=Path(temporary)/'source';baseline.mkdir();archive=subprocess.check_output(['git','-C',ROOT,'archive',evidence['baseline_source']])
  with tarfile.open(fileobj=io.BytesIO(archive)) as tar:tar.extractall(baseline,filter='data')
  subprocess.run(['git','-C',baseline,'init','-q'],check=True);subprocess.run(['git','-C',baseline,'add','.'],check=True);subprocess.run(['git','-C',baseline,'-c','user.name=Fixture','-c','user.email=fixture@localhost','commit','-qm','Immutable flag-off fixture'],check=True)
  options={'settings':['--nova-ui','--alarm-client','--denver','--quick-actions','--quick-radios'], 'springboard':['--alarm-client','--quick-actions','--denver'], 'file_browser':['--alarm-client','--quick-controls'], 'wifi_settings':['--alarm-client','--quick-actions','--nova-ui','--wifi-instance','15']}
  for name,opts in options.items():
   folders=[]
   for label,repo in [('baseline',baseline),('current',ROOT)]:
    dest=out/('watch-'+name+'-'+label);folders.append(dest);run(repo,name,opts,dest,out/(name+'-'+label+'.log'))
   for suffix in ['.elf','.json']:assert (folders[0]/(name+suffix)).read_bytes()==(folders[1]/(name+suffix)).read_bytes(),(name,suffix)
   evidence['watch'][name]={'exact_bytes':True,'sha256':hashlib.sha256((folders[1]/(name+'.elf')).read_bytes()).hexdigest()};print(name+' Watch flag-off byte identity PASS',flush=True)
(out/'builder-qualification.json').write_text(json.dumps(evidence,indent=2)+'\n')
