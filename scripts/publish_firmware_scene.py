#!/usr/bin/env python3
"""Publish immutable, verified canonical Firmware Update software closure."""
import argparse,json,os,re,urllib.error,urllib.parse,urllib.request,zipfile,tarfile
from pathlib import Path
from compose_shared_app_cohort import tree,encode,digest
from image_envelope import MAGIC,decode_image
from publish_touchpad_shared import require,archive
ROOT=Path(__file__).resolve().parents[1]
def verify(root,commit):
 require(re.fullmatch('[0-9a-f]{40}',commit),'Exact source commit required')
 require((root/'source/System.head').read_text().strip()==commit,'Source commit differs')
 lock=json.loads((ROOT/'docs/shared-firmware/source-lock.json').read_text())
 for role,sha in lock.items():require((root/'source'/f'{role}.head').read_text().strip()==sha,'Dependency source differs '+role)
 source_bytes={}
 for role in set(lock)|{'System'}:
  with tarfile.open(root/'source'/f'{role}.tar.gz') as tar:
   source_bytes[role]={m.name:tar.extractfile(m).read() for m in tar.getmembers() if m.isfile()}
 def check_source(path,expected,default=None):
  parts=Path(path).parts
  roles=[(i,'Utilities' if part=='FilesUtilities' else part) for i,part in enumerate(parts) if part in source_bytes or part=='FilesUtilities']
  if roles:i,role=roles[-1];relative='/'.join(parts[i+1:])
  else:role=default;relative=str(path)
  require(role in source_bytes and relative in source_bytes[role] and digest(source_bytes[role][relative])==expected,'Test/build source differs: '+str(path))
 baseline=json.loads((ROOT/'docs/shared-firmware/baseline.json').read_text())
 app=root/'application/canonical/ota_update.elf';sha=digest(app.read_bytes())
 build=json.loads((root/'application/build.json').read_text());assembly=json.loads((root/'assembly/assembly.json').read_text())
 require(build['repository_commit']==commit and not build['working_tree_dirty'],'Target source differs')
 require(build['utilities_commit']==lock['Utilities'] and not build['utilities_dirty'] and build['runtime_commit']==lock['Runtime'],'Target dependencies differ')
 for path,h in build['source_sha256'].items():check_source(path,h,'System')
 for name,record in build['headers'].items():check_source(record['source'],record['sha256'])
 require(len(build['builds'])==2 and all(r['sha256']==sha for r in build['builds']),'Canonical repeat differs')
 require(assembly['packed'] and assembly['shared_firmware_update']==sha and set(assembly['products'])=={'watch','wave','x4'},'Three canonical products required')
 files={'ota_update.elf':app,'application-build.json':root/'application/build.json','assembly.json':root/'assembly/assembly.json'}
 for lane,product in [('watch','twatch'),('wave','waveshare'),('x4','x4')]:
  selected=root/'application/selections'/product
  require(digest((selected/'ota_update.elf').read_bytes())==sha,'Selected ELF differs')
  require(digest((selected/'ota_update.json').read_bytes())==build['selections'][product]['manifest_sha256'],'Selected manifest differs')
  files[lane+'-ota_update.json']=selected/'ota_update.json'
  row=assembly['products'][lane];base=root/'assembly'/lane
  require(row['app_sha256']==sha and row['packed'] and row['store_roundtrip'] and row['sd_roundtrip'],'Pack proof missing')
  require(digest((base/'store.bin').read_bytes())==row['store_sha256'],'Packed store differs')
  require(digest((base/'sd-apps.zip').read_bytes())==row['sd_archive_sha256'],'Packed SD differs')
  before=tree(base/'before');after=tree(base/'after');stored=tree(base/'stored');sd=tree(base/'after-sd') if (base/'after-sd').exists() else {}
  oldsd=tree(base/'before-sd') if (base/'before-sd').exists() else {}
  require(set(before)==set(after)==set(stored),'Internal file inventory changed')
  require(all(after[n]==b for n,b in before.items() if n not in row['changed_internal']),'Unrelated internal bytes changed')
  require({n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}==after,'Canonical storage decoding differs')
  route=row['app_route'];require((sd if route['storage']=='sd' else after)[route['path']]==app.read_bytes(),'Installed ELF differs')
  original=baseline['authority'][lane]['applications'][0]['row'];expected=json.loads(before['boot.json'])
  require(row['original_app_row']==original and original in expected['app_capabilities'],'Original authority differs')
  expected['app_capabilities'][expected['app_capabilities'].index(original)]=row['app_row']
  oldprovider=row['original_provider_row'];newprovider=row['provider_row']
  require(oldprovider in expected['drivers'],'Original update provider absent')
  expected['drivers'][expected['drivers'].index(oldprovider)]=newprovider
  require(set(sd)==(set(oldsd)-set(row['removed_sd']))|set(row['added_sd']),'SD inventory differs')
  require(all(sd[n]==b for n,b in oldsd.items() if n not in set(row['changed_sd'])|set(row['removed_sd'])),'Unrelated SD bytes differ')
  if newprovider.get('image_source'):
   path=newprovider['image_source']['path']
   require(Path(path).name=='image-'+row['provider_sha256']+'.elf' and digest(sd[path])==row['provider_sha256'],'Content-addressed fallback differs')
  require(json.loads(after['boot.json'])==expected and after.get('cohort.json')==before.get('cohort.json'),'Unrelated authority/native metadata changed')
  for suffix in ('store.bin','sd-apps.zip'):files[lane+'-'+suffix]=base/suffix
  provider=root/'providers'/lane/'software-update-firmware'
  require(digest((provider/'driver.elf').read_bytes())==row['provider_sha256'],'Provider ELF differs')
  pr=json.loads((provider/'build-record.json').read_text());policy=dict(pr['update_policy']);policy.pop('header',None)
  require(pr['repository_commit']==commit and not pr['working_tree_dirty'] and pr['sha256']==row['provider_sha256'],'Provider build source differs')
  require(policy==baseline['authority'][lane]['provider_policy'] and '-DUPDATE_FIRMWARE=1' in pr['build_defines'],'Provider policy differs')
  for path,h in pr['source_sha256'].items():check_source(path,h,'System')
  for name in ('driver.elf','manifest.json','build-record.json'):files[lane+'-update-'+name]=provider/name
 negative=json.loads((root/'assembly-negative/receipt.json').read_text())
 require(negative['passed'] and len(negative['tests'])==19 and all(t['passed'] for t in negative['tests']),'Independent assembly cases missing')
 require(negative['assembly_sha256']==digest((root/'assembly/assembly.json').read_bytes()) and negative['application_sha256']==sha,'Independent assembly binding differs')
 for path,h in negative['source_sha256'].items():check_source(path,h)
 repeated=json.loads((root/'providers-repeat/receipt.json').read_text())
 require(repeated['passed'] and set(repeated['products'])=={'watch','wave','x4'},'Provider repeat proof missing')
 for lane in ('watch','wave','x4'):
  for key in ('store_sha256','sd_archive_sha256'):require(negative['products'][lane][key]==assembly['products'][lane][key],'Independent product binding differs')
  for name in ('driver.elf','manifest.json'):
   require(digest((root/'providers'/lane/'software-update-firmware'/name).read_bytes())==repeated['products'][lane][name]==digest((root/'providers-repeat'/lane/'software-update-firmware'/name).read_bytes()),'Provider repeat differs')
 for mode in ('normal','sanitized'):
  receipt=root/'admission'/mode/'receipt.json';r=json.loads(receipt.read_text())
  require(r['sanitized']==(mode=='sanitized') and not r['prepare_only'] and set(r['products'])=={'watch','wave','x4'},'Admission modes/lanes differ')
  require(r['product_binding']['source_commit']==commit and r['product_binding']['assembly_sha256']==digest((root/'assembly/assembly.json').read_bytes()),'Admission is not bound to this source/assembly')
  for lane,q in r['products'].items():
   require(len(q['results'])==1,'Admission result missing');result=q['results'][0]['result']
   require(result['prepared'] and result['cohort_validated'] and result['elf_count'] and not result['hardware_calls'] and not result['storage_calls'],'Admission failed')
  files['admission-'+mode+'.json']=receipt
 q=json.loads((root/'controller/receipt.json').read_text())
 require(q['passed'] and {r['sanitized'] for r in q['runs']}=={False,True} and len(q['runs'])==2 and all(r['passed'] for r in q['runs']),'Both controller test modes required')
 for path,h in q['source_sha256'].items():check_source(path,h)
 for name,h in q['runtime_headers_sha256'].items():check_source('sdk/app/'+name,h,'Runtime')
 for mode in ('normal','sanitized'):
  folder=root/'background'/mode
  q=json.loads((folder/'receipt.json').read_text())
  controller=json.loads((folder/'controller/receipt.json').read_text())
  require(q['sanitize']==(mode=='sanitized') and controller['sanitize']==(mode=='sanitized'),'Background test modes differ')
  require(len(q['commands'])>=3 and controller['result'] and controller['source_sha256'],'Background proof incomplete')
  for path,h in q['sources'].items():check_source(path,h)
  for path,h in controller['source_sha256'].items():
   if '/include/' in path and not any('/'+r+'/' in path for r in source_bytes):
    header=Path(path).name;origin=controller['header_origins'].get(header)
    require(origin,'Staged test header origin missing');check_source(origin,h)
   else:check_source(path,h)
  for name in ('ota_update_scene_background.c','ota_update_scene_background.h'):
   hashes=[h for path,h in q['sources'].items() if Path(path).name==name]
   require(hashes==[digest((ROOT/'Apps'/name).read_bytes())],'Background tested source differs')
 for asset in baseline['native_assets']:
  path=root/'downloads'/asset['name']
  require(path.stat().st_size==asset['size'] and digest(path.read_bytes())==asset['sha256'],'Native baseline differs')
  files[asset['name']]=path
 commits={p.stem:p.read_text().strip() for p in (root/'source').glob('*.head')}
 require(set(commits)==set(lock)|{'System'},'Source closure incomplete')
 for role in commits:require((root/'source'/f'{role}.tar.gz').is_file(),'Source archive missing')
 archive(root/'source.zip',[p for p in (root/'source').rglob('*') if p.is_file()],root/'source')
 evidence=[p for folder in root.iterdir() if folder.is_dir() and folder.name not in {'source','assembly','application','providers','downloads'} for p in folder.rglob('*') if p.is_file()]
 require(evidence,'Qualification evidence missing');archive(root/'qualification.zip',evidence,root)
 for name in ('source.zip','qualification.zip'):files[name]=root/name
 files['DELIVERY.md']=ROOT/'docs/shared-firmware/DELIVERY.md'
 receipt={'schema':1,'source_commit':commit,'sources':commits,'canonical_elf':sha,'products':assembly['products'],'baseline_release':baseline['release'],'physical_testing':'not performed','native_firmware':'unchanged','scope':'Actual controller/provider host tests, repeated target ELF, actual Runtime graph/internal-image admission and fixed-store/SD roundtrip; no target execution, external-media loading or live-heap proof.','assets':{n:{'sha256':digest(p.read_bytes()),'size':p.stat().st_size} for n,p in files.items()}}
 (root/'firmware-delivery.json').write_bytes(encode(receipt));files['firmware-delivery.json']=root/'firmware-delivery.json'
 return files
def run(root,publish,expected_source=None):
 repo=os.environ['GITHUB_REPOSITORY'];commit=expected_source or os.environ['GITHUB_SHA'];root=root.resolve()
 require(repo=='michaelrolphone-cmyk/RiscRTE-System-Apps','Owning repository required')
 files=verify(root,commit)
 plan={n:{'sha256':digest(p.read_bytes()),'size':p.stat().st_size} for n,p in files.items()}
 (root/'publication-plan.json').write_bytes(encode({'source_commit':commit,'assets':plan,'physical_testing':'not performed'}))
 if not publish:return
 token=os.environ['GITHUB_TOKEN']; base='https://api.github.com/repos/'+repo
 def request(url,body=None,method=None,binary=False,raw=False):
  headers={'Authorization':'Bearer '+token,'Accept':'application/octet-stream' if raw else 'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}
  if body is not None: headers['Content-Type']='application/octet-stream' if binary else 'application/json'
  data=body if binary else None if body is None else json.dumps(body).encode()
  with urllib.request.urlopen(urllib.request.Request(url,data=data,headers=headers,method=method),timeout=120) as response:
   result=response.read(); return result if raw else json.loads(result) if result else None
 def release_assets(release_id):
  result={};page=1
  while True:
   batch=request(base+'/releases/'+str(release_id)+'/assets?per_page=100&page='+str(page))
   for asset in batch:
    require(asset['name'] not in result,'Duplicate release asset name');result[asset['name']]=asset
   if len(batch)<100:return result
   page+=1
 tag='firmware-shared-'+commit
 try: release=request(base+'/releases/tags/'+tag)
 except urllib.error.HTTPError as error:
  if error.code!=404: raise
  release=request(base+'/releases',{'tag_name':tag,'target_commitish':commit,'name':'Shared Firmware Update '+commit[:12],'draft':True,'prerelease':True,'body':'One byte-identical Firmware Update ELF with optional shared scene and provider-owned update policy. Cumulative stores, unchanged native firmware, background behavior and unrelated data preserved. Source, qualification and installation limits included. Physical testing not performed.'})
 assets=release_assets(release['id'])
 require(set(assets).issubset(files),'Unexpected assets on immutable release')
 bindings=[]
 for name,path in files.items():
  data=path.read_bytes(); wanted=plan[name]
  asset=assets.get(name)
  if asset is None:
   require(release['draft'],'Never append to an already public release')
   asset=request(release['upload_url'].split('{')[0]+'?name='+urllib.parse.quote(name),data,binary=True)
  require(asset['size']==wanted['size'] and asset.get('digest')=='sha256:'+wanted['sha256'],'Immutable asset differs: '+name)
  require(digest(request(asset['url'],raw=True))==wanted['sha256'],'Authenticated asset bytes differ')
  bindings.append(dict(name=name,**wanted))
 if release['draft']: release=request(base+'/releases/'+str(release['id']),{'draft':False,'prerelease':True},method='PATCH')
 public=release_assets(release['id'])
 require(set(public)==set(files),'Public release asset set differs')
 for item in bindings:
  asset=public[item['name']]
  with urllib.request.urlopen(asset['browser_download_url'],timeout=120) as response: data=response.read()
  require(len(data)==item['size'] and digest(data)==item['sha256'],'Public bytes differ: '+item['name'])
  item['url']=asset['browser_download_url']
 receipt={'qualification_scope':'Store roundtrip and ELF/graph admission; no target instruction execution or live heap allocation proof','status':'public-bytes-verified','source_commit':commit,'release_url':release['html_url'],'assets':bindings,'physical_testing':'not performed'}
 (root/'publication.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps(receipt,indent=2))

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--publish',action='store_true');p.add_argument('--expected-source')
 a=p.parse_args();run(a.build,a.publish,a.expected_source)
