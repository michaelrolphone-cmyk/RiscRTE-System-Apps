#!/usr/bin/env python3
"""Publish the qualified shared Home closure. No component-only publication."""
import argparse, hashlib, json, os, re, urllib.error, urllib.parse, urllib.request
from pathlib import Path
def digest(data): return hashlib.sha256(data).hexdigest()
def require(ok, message):
 if not ok: raise ValueError(message)
def run(root, publish):
 repo=os.environ['GITHUB_REPOSITORY']; commit=os.environ['GITHUB_SHA']
 require(repo=='michaelrolphone-cmyk/RiscRTE-System-Apps','Owning repository required')
 require(re.fullmatch('[0-9a-f]{40}',commit),'Exact source commit required')
 root=root.resolve()
 def artifact(item):
  relative=Path(item['path'])
  require(not relative.is_absolute() and '..' not in relative.parts,'Unsafe artifact path')
  path=(root/relative).resolve()
  require(path.is_relative_to(root) and path.is_file(),'Artifact must be in the joined closure')
  data=path.read_bytes()
  require(digest(data)==item['sha256'] and len(data)==item['size'],'Artifact hash/size mismatch: '+str(relative))
  return path
 delivery=json.loads((root/'home-delivery.json').read_text())
 require(delivery['source_commit']==commit,'Exact delivery source required')
 elf=artifact(delivery['application']['elf']); manifest=artifact(delivery['application']['manifest'])
 require(elf.read_bytes()[:4]==b'\x7fELF','Actual ELF required')
 elf_hash=delivery['application']['elf']['sha256']
 files={'home.elf':elf,'home.json':manifest,'home-delivery.json':root/'home-delivery.json',
        'build.json':artifact(delivery['application']['build'])}
 build=json.loads(files['build.json'].read_text())
 require(build['shared_elf']==elf_hash and build['size_bytes']==delivery['application']['elf']['size'],'Build ELF binding mismatch')
 require(build['manifest_sha256']==delivery['application']['manifest']['sha256'],'Build manifest binding mismatch')
 require(build['views']==['native-faces','overview-retained-desk'],'Both owning semantic views required')
 assembly_path=artifact(delivery['assembly']); assembly=json.loads(assembly_path.read_text())
 require(assembly['packed'] is True and assembly['byte_identical_home']==elf_hash,'Complete packed common-ELF assembly required')
 require(set(assembly['products'])=={'watch','wave','x4'},'All three actual product lanes required')
 files['assembly.json']=assembly_path
 for lane,product in assembly['products'].items():
  require(product['home_sha256']==elf_hash and product['packed'] is True,'Per-product common ELF/packing missing')
  require(product['store_roundtrip'] and product['sd_roundtrip'],'Full store and SD roundtrip required')
  for field in ('store_sha256','sd_archive_sha256','inventory_sha256'):
   require(re.fullmatch('[0-9a-f]{64}',product[field]),'Missing product digest: '+lane+'/'+field)
 require(set(delivery['admissions'])=={'normal','sanitized'},'Both actual admission modes required')
 for mode,reference in delivery['admissions'].items():
  path=artifact(reference); receipt=json.loads(path.read_text())
  require(receipt.get('prepare_only',False) is False,'Prepare-only receipt cannot qualify delivery')
  require(receipt['schema']==1 and receipt['sanitized']==(mode=='sanitized'),'Admission mode/schema mismatch')
  require(set(receipt['products'])=={'watch','wave','x4'},'Missing admission lane')
  # These binding fields must be recorded by the invocation that prepared
  # the actual after trees. They cannot be inferred from compiler inputs.
  binding=receipt['product_binding']
  require(binding['assembly_sha256']==delivery['assembly']['sha256'],'Admission not bound to this assembly')
  require(binding['source_commit']==commit,'Admission source mismatch')
  require(set(receipt['candidate_input_files'])=={'watch','wave','x4'},'Exact candidate input binding required')
  for lane,product in receipt['products'].items():
   require(binding['products'][lane]['inventory_sha256']==assembly['products'][lane]['inventory_sha256'],'Admission inventory mismatch')
   inputs=receipt['candidate_input_files'][lane]
   require(set(inputs)=={'before','after','after-sd'},'Missing exact tree input map')
   for which,field in (('before','before_tree_sha256'),('after','after_tree_sha256'),('after-sd','after_sd_tree_sha256')):
    mapping=inputs[which]
    require(isinstance(mapping,dict),'Tree input map required')
    for tree_path,file_sha in mapping.items():
     require(not Path(tree_path).is_absolute() and '..' not in Path(tree_path).parts and re.fullmatch('[0-9a-f]{64}',file_sha),'Unsafe tree receipt')
    actual=digest((json.dumps(mapping,sort_keys=True,separators=(',',':'))+'\n').encode())
    require(actual==binding['products'][lane][field],'Actual tree input digest mismatch')
   require(product['inputs'] and product['per_tu_dependencies'],'Exact compiled source closure missing')
   after=[entry['result'] for entry in product['results'] if entry['input']=='after']
   require(len(after)==1,'Exactly one actual after-product result required')
   result=after[0]
   require(result['prepared'] and result['cohort_validated'] and result['elf_count']>0,'Actual broker/cohort admission failed')
   require(result['hardware_calls']==0 and result['storage_calls']==0,'Admission unexpectedly touched hardware/storage')
   if lane=='x4': require(result['retained_sd_serial_graph_bound'],'X4 retained SD/serial dependency closure missing')
  files['admission-'+mode+'.json']=path
 require(delivery['dependencies'],'Dependency artifact receipts required')
 for index,dependency in enumerate(delivery['dependencies']):
  # Preserve owning receipts verbatim; do not invent a replacement pass status.
  files['dependency-'+str(index)+'.json']=artifact(dependency['receipt'])
  require(dependency['artifacts'] and dependency['sources'] and dependency['licenses'],'Incomplete dependency closure')
  for category in ('artifacts','sources','licenses'):
   for item in dependency[category]:
    path=artifact(item)
    name='dependency-'+str(index)+'-'+category+'-'+item['sha256']+'-'+path.name
    files[name]=path
 for lane,assets in delivery.get('product_assets',{}).items():
  require(lane in assembly['products'],'Unknown product asset lane')
  for category,digest_field in (('store','store_sha256'),('sd','sd_archive_sha256')):
   item=assets[category]
   require(item['sha256']==assembly['products'][lane][digest_field],'Packed product asset differs: '+lane+'/'+category)
   files['product-'+lane+'-'+category+'-'+Path(item['path']).name]=artifact(item)
 for field in ('source_archive','source_lock','provenance','licenses','qualification'):
  files[field+Path(delivery[field]['path']).suffix]=artifact(delivery[field])
 source=json.loads(artifact(delivery['source_lock']).read_text())
 require(source['commits']['system_apps']==commit,'Source lock mismatch')
 names=list(files)
 require(len(names)==len(set(names)),'Duplicate assets')
 plan={name:{'sha256':digest(path.read_bytes()),'size':path.stat().st_size} for name,path in files.items()}
 (root/'publication-plan.json').write_text(json.dumps({'source_commit':commit,'assets':plan,'physical_testing':'not performed'},indent=2)+'\n')
 if not publish: return
 token=os.environ['GITHUB_TOKEN']; base='https://api.github.com/repos/'+repo
 def request(url,body=None,method=None,binary=False,raw=False):
  headers={'Authorization':'Bearer '+token,'Accept':'application/octet-stream' if raw else 'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}
  if body is not None: headers['Content-Type']='application/octet-stream' if binary else 'application/json'
  data=body if binary else None if body is None else json.dumps(body).encode()
  with urllib.request.urlopen(urllib.request.Request(url,data=data,headers=headers,method=method),timeout=120) as response:
   result=response.read(); return result if raw else json.loads(result) if result else None
 tag='home-shared-'+commit
 try: release=request(base+'/releases/tags/'+tag)
 except urllib.error.HTTPError as error:
  if error.code!=404: raise
  release=request(base+'/releases',{'tag_name':tag,'target_commitish':commit,'name':'Shared Home '+commit[:12],'draft':True,'prerelease':True,'body':'One byte-identical Home/Clock ELF with three-product store roundtrip and ELF/graph admission. Source, dependencies, admission, provenance and licenses included. Physical testing not performed.'})
 assets={a['name']:a for a in request(base+'/releases/'+str(release['id'])+'/assets')}
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
 public={a['name']:a for a in request(base+'/releases/'+str(release['id'])+'/assets')}
 for item in bindings:
  asset=public[item['name']]
  with urllib.request.urlopen(asset['browser_download_url'],timeout=120) as response: data=response.read()
  require(len(data)==item['size'] and digest(data)==item['sha256'],'Public bytes differ: '+item['name'])
  item['url']=asset['browser_download_url']
 receipt={'qualification_scope':'Store roundtrip and ELF/graph admission; no target instruction execution or live heap allocation proof','status':'public-bytes-verified','source_commit':commit,'release_url':release['html_url'],'assets':bindings,'physical_testing':'not performed'}
 (root/'publication.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps(receipt,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--publish',action='store_true')
 a=p.parse_args();run(a.build,a.publish)
