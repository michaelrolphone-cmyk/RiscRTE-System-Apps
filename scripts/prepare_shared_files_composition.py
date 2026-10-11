#!/usr/bin/env python3
"""Prepare a source-hashed store overlay and admission report, never firmware.
All existing apps/providers and their bytes survive. The output is explicitly
NOT admitted: native graph/import and resource-budget qualification must follow.
"""
import argparse,copy,hashlib,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for n in ('baseline','files','tcp','entropy','ble','exports','runtime','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--baseline-commit',required=True)
a=p.parse_args();root=a.baseline.resolve();out=a.output.resolve()
if out.exists():raise ValueError('Output must be new; never replace an existing composition')
def read(p):return json.loads(p.read_text())
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,indent=2)+'\n')
boot=read(root/'boot.json');before=copy.deepcopy(boot);manifest=read(a.files/'file_browser.json');record=read(a.files/'file_browser-build-record.json')
assert digest(a.files/'file_browser.elf')==record['sha256'],'Files ELF receipt mismatch'
old=read(root/'file_browser.json');pairs=lambda m:{(n['capability'],n['api']) for n in m['requires']}
assert pairs(old)<=pairs(manifest),'New Files would remove existing requirements'
row=next(x for x in boot['app_capabilities'] if x['manifest']=='file_browser.json')
old_grants=copy.deepcopy(row['grants'])
software={'storage.app-data.export','network.tcp.listener','crypto.entropy','bluetooth.session-setup'}
for grant in record['required_grants']:
 if grant in row['grants']:continue
 existing=[g for g in old_grants if (g['capability'],g['api'])==(grant['capability'],grant['api'])]
 # An unqualified hardware request retains the baseline's exact binding.
 # Runtime software service selectors remain strictly instance zero.
 if grant.get('instance_id',0)==0 and existing and grant['capability'] not in software:
  assert len(existing)==1,'Ambiguous baseline authority; explicit selector required'
  continue
 row['grants'].append(grant)
assert all(g.get('instance_id',0)==0 for g in row['grants'] if g['capability'] in software)
export=read(a.exports);assert 0<len(export['files'])<=16
# Verify existing exact owner/namespace authority before generating any output.
for entry in export['files']:
 owner=[read(root/x['manifest']) for x in boot['app_capabilities'] if read(root/x['manifest'])['id']==entry['owner']]
 assert len(owner)==1,'Unknown/duplicate export owner'
 policy=next(x for x in boot['app_capabilities'] if read(root/x['manifest'])['id']==entry['owner'])
 assert dict(capability='storage.app-data',api=1,instance_id=entry['namespace']) in policy['grants'],'Unowned export namespace'
row['app_data_export']=export
providers=[('sharing-tcp',a.tcp,'network.tcp.listener'),('sharing-entropy',a.entropy,'crypto.entropy'),('sharing-ble',a.ble,'bluetooth.session-setup')]
for name,src,cap in providers:
 m=read(src/'manifest.json');assert {'capability':cap,'api':1} in m['provides']
 proof=read(src/'build-record.json');assert digest(src/'driver.elf')==proof['sha256']
 assert not any(x['manifest']==name+'/manifest.json' for x in boot['drivers'])
 boot['drivers'].append({'manifest':name+'/manifest.json'})
assert all(g in row['grants'] for g in old_grants)
# Output stores are development artifacts, not firmware/installation bundles.
shutil.copytree(root,out/'store')
store=out/'store'
for name in ('file_browser.elf','file_browser.json'):shutil.copyfile(a.files/name,store/name)
for name,src,_ in providers:
 (store/name).mkdir()
 for filename in ('driver.elf','manifest.json'):shutil.copyfile(src/filename,store/name/filename)
write(store/'boot.json',boot)
preserved={str(f.relative_to(root)):digest(f) for f in root.rglob('*') if f.is_file() and f.relative_to(root).as_posix() not in ('boot.json','file_browser.json','file_browser.elf')}
assert all(digest(store/n)==sha for n,sha in preserved.items())
# Compile the current source limits directly, with discovery enabled. This does
# not silently select or change any firmware capacity setting.
probe=out/'limits.cpp';probe.write_text('#include "runtime/RuntimeLimits.h"\n#include <cstdio>\nint main(){std::printf("%zu %zu %zu",RiscLimits::Apps,RiscLimits::Providers,RiscLimits::Grants);}\n')
exe=out/'limits';subprocess.run(['c++','-DRISC_INSTALLED_APP_CAPACITY=40','-I'+str(a.runtime.resolve()/'src'),str(probe),'-o',str(exe)],check=True)
apps,providers_limit,grants_limit=map(int,subprocess.check_output([str(exe)],text=True).split())
count=len(boot['drivers']);needs=len(manifest['requires']);policy=len(row['grants'])
report={'status':'NOT_ADMITTED_NO_FIRMWARE','baseline_commit':a.baseline_commit,'baseline_boot_sha256':digest(root/'boot.json'),'files_sha256':record['sha256'],'runtime_commit':subprocess.check_output(['git','-C',str(a.runtime),'rev-parse','HEAD'],text=True).strip(),'apps_preserved':len(before['app_capabilities']),'providers_before':len(before['drivers']),'providers_required':count,'compiled_current_limits':{'apps':apps,'providers':providers_limit,'graph_grants':grants_limit},'files_policy_rows':policy,'files_requirement_rows':needs,'required_build_selections':{'RISC_APP_POLICY_ROWS':24 if policy>18 else 18 if policy>17 else 17 if policy>16 else 16,'RISC_APP_REQUIREMENT_ROWS':24 if needs>17 else 17 if needs>16 else 16},'preserved_files':preserved,'new_provider_inputs':{name:digest(src/'driver.elf') for name,src,_ in providers},'blockers':(['Current provider capacity is too small; focused capacity qualification required'] if count>providers_limit else [])+['Full native graph/import admission of all original plus new target ELFs','Peak active grants/RAM with Contexts, telemetry, Wi-Fi and BLE cleanup','Reconcile latest dynamic namespace/SD transaction source before final assembly','Physical WebDAV client / BLE interoperability and lifecycle qualification'],'network_export_scope':'explicit AppData map only; SD WebDAV mutation is not implemented','full_firmware_created':False}
write(out/'composition.json',report)
print(json.dumps({k:report[k] for k in ('status','apps_preserved','providers_required','compiled_current_limits','files_policy_rows','files_requirement_rows','blockers')},indent=2))
