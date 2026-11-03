#!/usr/bin/env python3
"""Publish a complete, byte-bound Touchpad software delivery after owning CI."""
import argparse,json,os,re,urllib.error,urllib.parse,urllib.request,zipfile
from pathlib import Path
from compose_shared_app_cohort import tree,encode,digest
from assemble_touchpad_delivery import BASELINE,AUTHORITY
def require(ok,message):
 if not ok:raise ValueError(message)
def archive(path,files,root):
 with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
  for f in sorted(files):
   require(not f.is_symlink(),'Symlink in evidence')
   i=zipfile.ZipInfo(f.relative_to(root).as_posix(),(1980,1,1,0,0,0));i.compress_type=zipfile.ZIP_DEFLATED;i.external_attr=0o100644<<16;z.writestr(i,f.read_bytes())
def verify(root,commit):
 require(re.fullmatch('[0-9a-f]{40}',commit),'Exact System source required')
 require((root/'source/System.head').read_text().strip()==commit,'System source mismatch')
 require((root/'source/Utilities.head').read_text().strip()=='286f70ef6ef9b2f7142a37d45fcc7717312c77df','Utilities source mismatch')
 assembly_path=root/'assembly/assembly.json';assembly=json.loads(assembly_path.read_text())
 require(assembly['packed'] is True and set(assembly['products'])=={'watch','wave','x4'},'Three packed lanes required')
 package=root/'application/packages/ble_touchpad';elf=package/'ble_touchpad.elf';manifest=package/'ble_touchpad.json';sha=digest(elf.read_bytes())
 build=json.loads((root/'application/packages.json').read_text())
 require(len(build['packages'])==1 and build['packages'][0]['sha256']==sha and build['packages'][0]['reproducibility']['sha256']==sha,'Canonical repeat differs')
 require(assembly['byte_identical_touchpad']==sha and elf.read_bytes().startswith(b'\x7fELF'),'Assembly ELF differs')
 require(not json.loads(manifest.read_text())['requires'] and not build['application_defines'],'App is not optional/device-neutral')
 require(set(build['packages'][0]['selections'])=={'x4','twatch','waveshare'},'Missing device selection')
 for row in build['packages'][0]['selections'].values():require(digest((root/'application'/row['path']).read_bytes())==sha,'Device selection differs')
 files={'ble_touchpad.elf':elf,'ble_touchpad.json':manifest,'application-build.json':root/'application/packages.json','assembly.json':assembly_path}
 for lane,a in assembly['products'].items():
  spec=AUTHORITY[lane];base=root/'assembly'/lane
  require(a['touchpad_sha256']==sha and a['packed'] and a['store_roundtrip'] and a['sd_roundtrip'],'Roundtrip evidence missing')
  original=tree(base/'before');after=tree(base/'after');oldstored=tree(base/'before-stored');stored=tree(base/'stored')
  oldsd=tree(base/'before-sd') if (base/'before-sd').exists() else {};sd=tree(base/'after-sd')
  require(a['original_app_row']==spec['app_row'],'Original app authority differs')
  expected=json.loads(original['boot.json']);require(spec['app_row'] in expected['app_capabilities'],'Original policy absent')
  expected['app_capabilities'][expected['app_capabilities'].index(spec['app_row'])]=a['app_row']
  require(json.loads(after['boot.json'])==expected,'Unrelated authority differs')
  allowed={a['installed_manifest'],'ui-scene/manifest.json','ui-scene/driver.elf','boot.json'}
  if a['application_route']['storage']=='internal':allowed.add(a['application_route']['path'])
  require(set(a['changed_internal'])<=allowed,'Unrelated changes allowed by receipt')
  require(set(after)==set(original) and set(stored)==set(oldstored) and set(sd)==set(oldsd),'File inventories differ')
  require(all(after[n]==b for n,b in original.items() if n not in a['changed_internal']),'Unrelated internal bytes differ')
  require(all(stored[n]==b for n,b in oldstored.items() if n not in a['changed_internal']),'Unrelated stored bytes differ')
  require(all(sd[n]==b for n,b in oldsd.items() if n not in a['changed_sd']),'Unrelated SD bytes differ')
  require(original['cohort.json']==after['cohort.json'] and digest(after['cohort.json'])==a['native_metadata_sha256'],'Native metadata differs')
  route=a['application_route'];require((sd if route['storage']=='sd' else after)[route['path']]==elf.read_bytes(),'Installed canonical ELF differs')
  for key,name in (('store_sha256','store.bin'),('sd_archive_sha256','sd-apps.zip')):
   f=base/name;require(digest(f.read_bytes())==a[key],'Packed asset differs');files[lane+'-'+name]=f
  for kind in ('descriptor','qualification','firmware'):
   name='native-'+lane+'-'+kind+('.bin' if kind=='firmware' else '.json');f=root/'assembly/downloads'/name;ref=BASELINE[name]
   require(digest(f.read_bytes())==ref['sha256'] and f.stat().st_size==ref['size'],'Native dependency differs');files[name]=f
  profile=a['scene_profile'];provider=root/'providers'/profile/'scene-host';pr=json.loads((provider/'build.json').read_text())
  require(digest((provider/'driver.elf').read_bytes())==a['scene_sha256']==pr['sha256']==pr['repeat_sha256'],'Provider repeat differs')
  for f in provider.rglob('*'):
   if f.is_file():files['scene-'+profile+'-'+f.relative_to(provider).as_posix().replace('/','-')]=f
  for mode in ('normal','sanitized'):
   path=root/'admission'/mode/'receipt.json';q=json.loads(path.read_text());b=q['product_binding']
   require(not q['prepare_only'] and q['sanitized']==(mode=='sanitized') and b['source_commit']==commit and b['assembly_sha256']==digest(assembly_path.read_bytes()),'Admission binding differs')
   require(b['products'][lane]['inventory_sha256']==a['inventory_sha256'],'Inventory admission differs')
   inputs={'before':original,'after':after,'stored':stored,'after-sd':sd}
   require(q['candidate_input_files'][lane]=={k:{n:digest(v) for n,v in entries.items()} for k,entries in inputs.items()},'Admitted input bytes differ')
   rows=q['products'][lane]['results'];require(len(rows)==1 and rows[0]['input']=='after','Actual candidate result absent')
   r=rows[0]['result'];require(r['prepared'] and r['cohort_validated'] and r['elf_count']>0 and not r['hardware_calls'] and not r['storage_calls'],'Invalid admission')
   files['admission-'+mode+'.json']=path
 for mode in ('normal','sanitized'):
  q=json.loads((root/('runtime-'+mode)/'results.json').read_text())
  require(q['sanitize']==(mode=='sanitized') and len(q['cases'])==15 and all(x['passed'] for x in q['cases']),'Actual app lifecycle cases missing')
  q=json.loads((root/('pointer-'+mode)/'results.json').read_text())
  require(q['sanitize']==(mode=='sanitized') and len(q['passed'])>=455 and any(x['case']=='confirm_visible' for x in q['passed']),'Visible-confirmation evidence missing')
  q=json.loads((root/('hold-'+mode)/'results.json').read_text())
  require(q['sanitize']==(mode=='sanitized') and any(x['case']=='resume' for x in q['cases']) and any(x['case']=='confirm_resume' for x in q['cases']),'Checkpoint regression evidence missing')
  for name in ('canvas','keypad','focused','background','controller','original'):require((root/(name+'-'+mode)).is_dir(),'Regression closure missing '+name)
 commits={f.stem:f.read_text().strip() for f in (root/'source').glob('*.head')}
 archives={f.name:{'sha256':digest(f.read_bytes()),'size':f.stat().st_size} for f in (root/'source').glob('*.tar.gz')}
 require(len(commits)==len(archives)==8,'Owning source archives missing')
 expected={Path(line.split()[1]).name:line.split()[0] for line in (root/'source/archives.sha256').read_text().splitlines()}
 require(expected=={n:r['sha256'] for n,r in archives.items()},'Original source archive digest differs')
 (root/'source-lock.json').write_bytes(encode({'commits':commits,'archives':archives}))
 archive(root/'source.zip',[p for p in (root/'source').rglob('*') if p.is_file()],root/'source')
 evidence=[p for folder in root.iterdir() if folder.is_dir() and folder.name not in {'source','assembly','application','providers'} for p in folder.rglob('*') if p.is_file()]
 require(evidence,'Qualification closure missing');archive(root/'qualification.zip',evidence,root)
 for name in ('source-lock.json','source.zip','qualification.zip'):files[name]=root/name
 guide=Path(__file__).resolve().parents[1]/'docs/shared-touchpad/DELIVERY.md';require(guide.is_file(),'Delivery guide absent');files['DELIVERY.md']=guide
 receipt={'schema':1,'source_commit':commit,'sources':commits,'canonical_elf':sha,'products':assembly['products'],'baseline':BASELINE,
 'scope':'Host actual Runtime/app/provider interaction, target ELF validation, product graph and internal-image admission, fixed-store/SD roundtrip and unrelated-byte preservation. External SD routes are byte-bound, not physical media execution. No physical pairing, latency, interoperability, heap or device-test claim.',
 'physical_testing':'not performed','native_firmware':'unchanged from published Home baseline',
 'assets':{n:{'sha256':digest(f.read_bytes()),'size':f.stat().st_size} for n,f in files.items()}}
 (root/'touchpad-delivery.json').write_bytes(encode(receipt));files['touchpad-delivery.json']=root/'touchpad-delivery.json'
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
 tag='touchpad-shared-'+commit
 try: release=request(base+'/releases/tags/'+tag)
 except urllib.error.HTTPError as error:
  if error.code!=404: raise
  release=request(base+'/releases',{'tag_name':tag,'target_commitish':commit,'name':'Shared BLE Touchpad '+commit[:12],'draft':True,'prerelease':True,'body':'One byte-identical BLE Touchpad ELF and reusable scene pointer provider. Cumulative Home stores, native firmware and unrelated data preserved. Source, qualification and installation limits included. Physical testing not performed.'})
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
