#!/usr/bin/env python3
"""Inventory storage contracts and mechanism coupling across supplied repositories.
This is an evidence report, not a blanket allowlist or proof of migration.
Usage: audit_storage_consumers.py --repo NAME=PATH ... --output REPORT.json
"""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--repo',action='append',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
patterns={
 'physical_api':r'\b(?:SPIFFS|LittleFS|SD_MMC|SD|SdFat|sdmmc_card_t|nvs_open|esp_vfs_spiffs_register)\b',
 'physical_path':r'[/](?:sd|spiffs|sdcard|usb)(?:/|["\'])',
 'legacy_file_contract':r'(?:storage\.(?:volume|installed-files|user-files|app-data\.export)|RiscStorageVolume|T5StorageApi|T5FileBrowserApi)',
 'generic_file_contract':r'(?:storage\.files|RiscStorageFiles)',
 'persistence_contract':r'(?:storage\.(?:app-data|key-value)|RiscAppData|RiscKeyValue|fopen\s*\()',
 'block_transport_contract':r'(?:usb\.device\.msc|RiscStorageExport|RiscUsbDeviceMsc)',
}
regex={k:re.compile(v) for k,v in patterns.items()};report={'schema':1,'scope':'Optional generic storage capability across all applications and device classes. No mandatory storage requirement.','repositories':[]}
for item in a.repo:
 name,path=item.split('=',1);root=Path(path);entry={'repository':name,'commit':None,'source_mode':'working tree; base commit recorded; source lines and hashes are the observed evidence','files':[]}
 try:
  entry['commit']=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
  names=subprocess.check_output(['git','-C',str(root),'ls-files'],text=True).splitlines()
 except subprocess.CalledProcessError:
  names=[str(x.relative_to(root)) for x in root.rglob('*') if x.is_file()]
 for rel in names:
  f=Path(rel)
  if f.suffix not in ('.h','.hpp','.c','.cpp','.inc','.json'):continue
  if any(x.lower() in {'test','tests','build','docs','receipts','evidence','.git','.pio','vendor','third_party','third-party','node_modules'} or x.startswith('build-') for x in f.parts):continue
  if not (root/f).is_file():continue
  hits=[]
  for n,line in enumerate((root/f).read_text(errors='replace').splitlines(),1):
   tags=[k for k,v in regex.items() if v.search(line)]
   if tags:hits.append({'line':n,'kinds':tags,'source':line.strip()[:320]})
  if hits:
   layer='provider' if f.parts[0].lower() in {'drivers','providers','services'} else 'application' if f.parts[0].lower()=='apps' else 'shared_or_runtime'
   entry['files'].append({'path':rel,'sha256':hashlib.sha256((root/f).read_bytes()).hexdigest(),'layer':layer,'hits':hits})
 report['repositories'].append(entry)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({x['repository']:{'commit':x['commit'],'matching_files':len(x['files'])} for x in report['repositories']},indent=2))
