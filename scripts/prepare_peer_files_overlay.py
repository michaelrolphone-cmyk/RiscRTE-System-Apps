#!/usr/bin/env python3
"""Copy a qualified source store and replace only explicit peer Files inputs.
No firmware, SD content, settings, admission rules or native capacity is changed.
"""
import argparse,hashlib,json,pathlib,shutil
p=argparse.ArgumentParser(description=__doc__)
for key in ('baseline','files','tcp','ble','output'):p.add_argument('--'+key,type=pathlib.Path,required=True)
p.add_argument('--exports',type=pathlib.Path)
a=p.parse_args();assert not a.output.exists(),'Use a fresh isolated output'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text())
boot=read(a.baseline/'boot.json');manifest=read(a.files/'file_browser.json');receipt=read(a.files/'file_browser-build-record.json')
assert manifest['version']=='1.5.25' and sha(a.files/'file_browser.elf')==receipt['sha256']
assert '-DPORTABLE_FILE_PEER' in receipt['defines']
row=next(r for r in boot['app_capabilities'] if r['manifest']=='file_browser.json')
old=read(a.baseline/'file_browser.json');pairs=lambda m:{(r['capability'],r['api']) for r in m['requires']}
assert pairs(old)<=pairs(manifest),'Inherited Files capability removed'
assert pairs(manifest)<={(g['capability'],g['api']) for g in row['grants']},'Missing explicit Files grant'
software={'storage.installed-files','storage.app-data.export','network.tcp.listener','crypto.entropy','bluetooth.session-setup','file.open'}
assert all(g.get('instance_id',0)==0 for g in row['grants'] if g['capability'] in software)
if a.exports:
 policy=read(a.exports);assert 0<len(policy['files'])<=16
 for entry in policy['files']:
  owners=[r for r in boot['app_capabilities'] if read(a.baseline/r['manifest'])['id']==entry['owner']]
  assert len(owners)==1
  assert dict(capability='storage.app-data',api=1,instance_id=entry['namespace']) in owners[0]['grants']
 row['app_data_export']=policy
selected=[('network.tcp.listener',a.tcp),('bluetooth.session-setup',a.ble)];replacements={'file_browser.json':a.files/'file_browser.json','file_browser.elf':a.files/'file_browser.elf'}
for capability,source in selected:
 provider=read(source/'manifest.json');proof=read(source/'build-record.json')
 assert sha(source/'driver.elf')==proof['sha256'] and dict(capability=capability,api=1) in provider['provides']
 owners=[r for r in boot['drivers'] if dict(capability=capability,api=1) in read(a.baseline/r['manifest'])['provides']]
 assert len(owners)==1
 path=pathlib.PurePosixPath(owners[0]['manifest']).parent
 for name in ('manifest.json','driver.elf'):replacements[str(path/name)]=source/name
shutil.copytree(a.baseline,a.output/'store')
for name,source in replacements.items():shutil.copyfile(source,a.output/'store'/name)
(a.output/'store/boot.json').write_text(json.dumps(boot,indent=2)+'\n')
preserved={str(f.relative_to(a.baseline)):sha(f) for f in a.baseline.rglob('*') if f.is_file() and str(f.relative_to(a.baseline)) not in {*replacements,'boot.json'}}
assert all(sha(a.output/'store'/name)==value for name,value in preserved.items())
record=dict(status='SOURCE_STORE_ONLY_REQUIRES_CURRENT_NATIVE_CONNECTOR_AND_ADMISSION',firmware_created=False,providers=len(boot['drivers']),apps=len(boot['app_capabilities']),files_policy_rows=len(row['grants']),preserved=preserved,replacements={n:sha(s) for n,s in replacements.items()},boot_sha256=sha(a.output/'store/boot.json'),existing_user_exports=row.get('app_data_export'),native_dependency='Runtime TCP connector 1a1bd64629081a64de7e582adab51292178b1519 plus current installer lineage; rebuild native source, never pair with an old listener-only binary')
(a.output/'composition.json').write_text(json.dumps(record,indent=2)+'\n');print(record['status'],record['providers'],'providers',record['apps'],'apps')
