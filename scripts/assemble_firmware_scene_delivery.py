#!/usr/bin/env python3
"""Replace OTA app/provider in exact published products; preserve unrelated bytes."""
import argparse,copy,io,json,sys,urllib.request,zipfile
from pathlib import Path,PurePosixPath
from compose_shared_app_cohort import safe,encode,digest,write_tree
from assemble_shared_home_cohort import inventory,fingerprint,pack_candidate
from image_envelope import MAGIC,decode_image,encode_image,receipt as image_receipt
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('application','providers','bootfs-tools','output'):
  p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--downloads',type=Path)
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
 sys.path.insert(0,str(a.bootfs_tools.resolve()))
 from read_only_spiffs import read_image
 from current_bootfs import build
 baseline=json.loads((ROOT/'docs/shared-firmware/baseline.json').read_text())
 downloads=a.downloads or a.output/'downloads';downloads.mkdir(parents=True,exist_ok=True)
 for row in baseline['assets']:
  path=downloads/row['name']
  if not path.exists():
   with urllib.request.urlopen(row['url'],timeout=180) as r:path.write_bytes(r.read())
  if path.stat().st_size!=row['size'] or digest(path.read_bytes())!=row['sha256']:raise ValueError('Baseline bytes differ '+row['name'])
 app=(a.application/'canonical/ota_update.elf').read_bytes()
 manifest=json.loads((a.application/'selections/x4/ota_update.json').read_text())
 record=json.loads((a.application/'build.json').read_text())
 if not app.startswith(b'\x7fELF') or manifest['requires'] or manifest['id']!='ota_update':raise ValueError('Canonical optional app required')
 if any(r['sha256']!=digest(app) for r in record['builds']) or any(r['sha256']!=digest(app) for r in record['selections'].values()):raise ValueError('Application build/selection differs')
 results={}
 for lane in ('watch','wave','x4'):
  selection={'watch':'twatch','wave':'waveshare','x4':'x4'}[lane]
  manifest_bytes=(a.application/'selections'/selection/'ota_update.json').read_bytes()
  if digest(manifest_bytes)!=record['selections'][selection]['manifest_sha256']:raise ValueError('Product manifest changed')
  manifest=json.loads(manifest_bytes)
  declarations={(d['capability'],d['api']) for d in manifest['optional']}
  raw=(downloads/(lane+'-store.bin')).read_bytes();old_stored=read_image(raw,len(raw))
  before={n:decode_image(b) if b.startswith(MAGIC) else b for n,b in old_stored.items()}
  with zipfile.ZipFile(downloads/(lane+'-sd-apps.zip')) as z:
   if len(z.namelist())!=len(set(z.namelist())):raise ValueError('Duplicate SD name')
   old_sd={safe(n):z.read(n) for n in z.namelist() if not n.endswith('/')}
  out=dict(before);sd=dict(old_sd);boot=json.loads(before['boot.json']);original_boot=copy.deepcopy(boot)
  authority=baseline['authority'][lane];apps=authority['applications']
  if len(apps)!=1:raise ValueError('Original OTA authority ambiguous')
  original=apps[0]['row'];name=original['manifest'];installed=apps[0]['manifest']
  if original not in boot['app_capabilities'] or json.loads(before[name])!=installed:raise ValueError('Original OTA authority differs')
  row=copy.deepcopy(original)
  # Physical input moves to the already installed scene provider. Display output
  # remains optional for existing Context brightness applicability observation.
  row['grants']=[g for g in row['grants'] if g['capability'] not in ('input.touch.raw','input.navigation')]
  for capability in ('ui.scene','time.civil'):
   owners=[]
   for provider in boot['drivers']:
    m=json.loads(before[provider['manifest']])
    if {'capability':capability,'api':1} in m.get('provides',[]):owners.append(provider)
   if len(owners)!=1 or owners[0].get('instance_id',0)!=0:raise ValueError('Installed logical provider is not unique '+capability)
   grant={'capability':capability,'api':1,'instance_id':0}
   if grant not in row['grants']:row['grants'].append(grant)
  catalog_grant={'capability':'app.catalog','api':1,'instance_id':0}
  contexts=[r for r in boot['app_capabilities'] if r['manifest']=='contexts.json']
  if len(contexts)!=1 or catalog_grant not in contexts[0]['grants']:raise ValueError('Existing native catalog authority not established')
  if catalog_grant not in row['grants']:row['grants'].append(catalog_grant)
  if any((g['capability'],g['api']) not in declarations for g in row['grants']):raise ValueError('Existing app grant undeclared '+lane+': '+repr([g for g in row['grants'] if (g['capability'],g['api']) not in declarations]))
  if len(row['grants'])>32:raise ValueError('Owning app-policy capacity exceeded')
  boot['app_capabilities'][boot['app_capabilities'].index(original)]=row
  new_manifest=copy.deepcopy(installed)
  for key in ('requires','optional','version','entry','architecture'):new_manifest[key]=copy.deepcopy(manifest[key])
  out[name]=encode(new_manifest)
  route=original.get('image_source');app_path=safe(route['path']) if route else safe(str(PurePosixPath(name).parent/installed['file_name']))
  target=sd if route else out
  if app_path not in target:raise ValueError('Original application route missing')
  target[app_path]=app;changed={name,'boot.json'};sd_changed=set();sd_removed=set();sd_added=set()
  if route:
   sidecar=str(PurePosixPath(app_path).parent/'manifest.json')
   if sidecar not in sd or json.loads(sd[sidecar])!=installed:raise ValueError('Original SD application sidecar differs')
   sd[sidecar]=out[name];sd_changed.update((app_path,sidecar))
  else:changed.add(app_path)
  providers=[p for p in authority['providers'] if p['manifest']['id']=='software-update-firmware']
  if len(providers)!=1:raise ValueError('Original update provider ambiguous')
  old=providers[0];p_row=old['row'];p_name=p_row['manifest'];p_path=str(PurePosixPath(p_name).parent/'driver.elf')
  if p_row not in boot['drivers'] or json.loads(before[p_name])!=old['manifest']:raise ValueError('Provider authority changed')
  package=a.providers/lane/'software-update-firmware';p_manifest=json.loads((package/'manifest.json').read_text());p_bytes=(package/'driver.elf').read_bytes();p_record=json.loads((package/'build-record.json').read_text())
  if p_record['sha256']!=digest(p_bytes) or p_manifest['version']!='0.1.7':raise ValueError('Provider source/build identity differs')
  policy=dict(p_record['update_policy']);policy.pop('header',None)
  if policy!=authority['provider_policy']:raise ValueError('Provider policy belongs to another product '+lane)
  if '-DUPDATE_FIRMWARE=1' not in p_record['build_defines']:raise ValueError('Firmware-only provider required')
  if {k:v for k,v in p_manifest.items() if k!='version'}!={k:v for k,v in old['manifest'].items() if k!='version'}:raise ValueError('Provider contract changed '+lane)
  out[p_name]=encode(p_manifest);out[p_path]=p_bytes;changed.update((p_name,p_path))
  source=p_row.get('image_source')
  new_p_row=copy.deepcopy(p_row)
  if source:
   path=safe(source['path'])
   if not source.get('internal_fallback') or path not in sd:raise ValueError('Original provider SD fallback absent')
   if PurePosixPath(path).name!='image-'+digest(before[p_path])+'.elf':raise ValueError('Original content-addressed fallback differs')
   new_path=str(PurePosixPath(path).parent/('image-'+digest(p_bytes)+'.elf'))
   if new_path!=path and new_path in sd:raise ValueError('New provider route collides')
   del sd[path];sd[new_path]=p_bytes;sd_removed.add(path);sd_added.add(new_path)
   new_p_row['image_source']['path']=new_path
   boot['drivers'][boot['drivers'].index(p_row)]=new_p_row
  out['boot.json']=encode(boot)
  restored=copy.deepcopy(boot);restored['app_capabilities'][restored['app_capabilities'].index(row)]=original
  restored['drivers'][restored['drivers'].index(new_p_row)]=p_row
  if restored!=original_boot:raise ValueError('Unrelated boot authority changed')
  if set(out)!=set(before) or set(sd)!=(set(old_sd)-sd_removed)|sd_added:raise ValueError('Unrelated file set changed')
  if any(out[n]!=b for n,b in before.items() if n not in changed) or any(sd[n]!=b for n,b in old_sd.items() if n not in sd_changed|sd_removed):raise ValueError('Unrelated bytes changed')
  stored=dict(old_stored);encoded={}
  for n,b in out.items():
   if b!=before[n]:stored[n]=encode_image(b) if old_stored[n].startswith(MAGIC) else b
   if stored[n].startswith(MAGIC):encoded[n]=image_receipt(b,stored[n])
  if {n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}!=out:raise ValueError('Canonical recovery differs')
  directory=a.output/lane
  for kind,entries in (('before',before),('before-stored',old_stored),('before-sd',old_sd),('after',out),('stored',stored),('after-sd',sd)):
   (directory/kind).mkdir(parents=True,exist_ok=True);write_tree(directory/kind,entries)
  result={'inventory_sha256':fingerprint(inventory(before,old_sd)),'app_sha256':digest(app),'app_row':row,'original_app_row':original,'app_route':{'storage':'sd' if route else 'internal','path':app_path},'provider_sha256':digest(p_bytes),'provider_policy':p_record['update_policy'],'original_provider_row':p_row,'provider_row':new_p_row,'changed_internal':sorted(n for n in changed if out[n]!=before[n]),'changed_sd':sorted(n for n in sd_changed if sd[n]!=old_sd[n]),'removed_sd':sorted(sd_removed),'added_sd':sorted(sd_added),'native_metadata_unchanged':out.get('cohort.json')==before.get('cohort.json'),'physical_execution':False}
  if encoded:result['image_storage']={'schema':1,'format':'RTEIMG1','images':encoded,'canonical_files':{n:digest(b) for n,b in sorted(out.items())},'stored_files':{n:digest(b) for n,b in sorted(stored.items())},'canonical_recovery_verified':True}
  result.update(pack_candidate(directory,stored,sd,len(raw),build,read_image));results[lane]=result
 (a.output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'shared_firmware_update':digest(app),'packed':True,'baseline_release':baseline['release']}))
 print('Canonical OTA bytes and preserved fixed-size products packed:',digest(app))
if __name__=='__main__':main()
