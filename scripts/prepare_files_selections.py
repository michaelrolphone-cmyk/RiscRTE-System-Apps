#!/usr/bin/env python3
"""Prepare canonical Files selections and reviewable scene-only policy deltas.
No boot/store mutation or claim of complete product admission.
"""
import argparse,copy,hashlib,json,shutil,subprocess
from pathlib import Path
from link_files_scene import make_manifest,verified_linked_manifest
p=argparse.ArgumentParser(description=__doc__)
for name in ('authority','linked','parser','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
authority=json.loads(a.authority.read_text())['results']['record']['payload']['inline']['products'];link=json.loads((a.linked/'link-receipt.json').read_text());elf=a.linked/'file_browser.elf';digest=hashlib.sha256(elf.read_bytes()).hexdigest();assert digest==link['sha256'] and link['repeat_identical']
canonical=verified_linked_manifest((a.linked/'file_browser.json').read_bytes(),link)
receipts={}
for name,original in authority.items():
 target=o/name;target.mkdir();old=original['manifests']['file_browser.json'];alarm=next(g['api'] for g in original['rows'][0]['grants'] if g['capability']=='alarm.service');new=make_manifest(canonical['version'],alarm)
 # Bind optional declarations to admitted authority, preserving all original
 # grant pairs. This also respects retained Runtime declaration capacities.
 admitted={(g['capability'],g['api']) for row in original['rows'] for g in row['grants']}|{('ui.scene',1)}
 new['optional']=[d for d in new['optional'] if (d['capability'],d['api']) in admitted]
 assert {(d['capability'],d['api']) for d in new['optional']}==admitted
 (target/'file_browser.json').write_text(json.dumps(new,indent=2)+'\n');shutil.copyfile(elf,target/'file_browser.elf');assert hashlib.sha256((target/'file_browser.elf').read_bytes()).hexdigest()==digest
 declared={(r['capability'],r['api']) for r in new['optional']};before=original['rows'];after=copy.deepcopy(before)
 for row in after:
  assert all((g['capability'],g['api']) in declared for g in row['grants'])
  assert not any(g['capability']=='ui.scene' for g in row['grants'])
  row['grants'].append({'capability':'ui.scene','api':1,'instance_id':0})
 for b,n in zip(before,after):
  check=copy.deepcopy(n);assert check['grants'].pop()=={'capability':'ui.scene','api':1,'instance_id':0};assert check==b
 if name=='x4':assert not any(g['capability'] in ('contexts.service','storage.shared-data','rtc.clock','file.open.source') for row in after for g in row['grants'])
 (target/'original-policy-rows.json').write_text(json.dumps(before,indent=2)+'\n');(target/'proposed-policy-rows.json').write_text(json.dumps(after,indent=2)+'\n')
 result=subprocess.run([str(a.parser.resolve()),str(target/'file_browser.json')],capture_output=True,text=True,check=True);(target/'declaration-parser.log').write_text(result.stdout+result.stderr)
 receipts[name]={'old_version':old['version'],'new_version':new['version'],'elf_sha256':digest,'elf_bytes':elf.stat().st_size,'alarm_api':alarm,'original_boot_sha256':original['boot_sha256'],'policy_delta':'ui.scene API1 instance0 only; all existing grants, namespace/export maps and row fields preserved','storage_authority_changed':False,'data_files_touched':False}
assert len({r['elf_sha256'] for r in receipts.values()})==1
(o/'receipt.json').write_text(json.dumps({'scope':__doc__,'authority_sha256':hashlib.sha256(a.authority.read_bytes()).hexdigest(),'link_receipt_sha256':hashlib.sha256((a.linked/'link-receipt.json').read_bytes()).hexdigest(),'selections':receipts,'actual_product_admission':'pending complete original boot/provider/store inputs in parent lane','hardware':'not_tested'},indent=2)+'\n')
print('Three canonical selections: byte-identical ELF; Runtime declaration parsing and exact scene-only policy deltas PASS')
