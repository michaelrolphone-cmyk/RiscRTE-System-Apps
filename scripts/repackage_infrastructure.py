#!/usr/bin/env python3
"""Replace product ELFs while reusing a verified, unchanged native runtime release."""
import argparse,copy,hashlib,json,pathlib,re,shutil,subprocess,sys,zipfile
from infrastructure_fixtures import generate
from install_infrastructure import verify
from package_infrastructure import ROOT,sha,commit
from provision_infrastructure import seed

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('base-bundle','base-archive','runtime','elf','spiffsgen','output','evidence'):
  p.add_argument('--'+name,type=pathlib.Path,required=True)
 p.add_argument('--base-archive-sha256',required=True);p.add_argument('--base-release-url',required=True)
 a=p.parse_args();base=a.base_bundle.resolve();r=a.runtime.resolve();out=a.output.resolve()
 if sha(a.base_archive)!=a.base_archive_sha256:raise ValueError('Published base archive digest mismatch')
 verify(base);old=json.loads((base/'flash-manifest.json').read_text())
 with zipfile.ZipFile(a.base_archive) as z:
  prefix=base.name+'/'
  for f in old['files']:
   if z.read(prefix+f['path'])!=(base/f['path']).read_bytes():raise ValueError('Base extraction mismatch: '+f['path'])
  if z.read(prefix+'flash-manifest.json')!=(base/'flash-manifest.json').read_bytes():raise ValueError('Base manifest mismatch')
 if old['source']['runtime']!=commit(r):raise ValueError('Native reuse requires unchanged Runtime source')
 fw=(base/'firmware.bin').read_bytes()
 if set(re.findall(rb'RTE_SOURCE=([0-9a-f]{40})\x00',fw))!={commit(r).encode()}:raise ValueError('Native binary source mismatch')
 builds=json.loads((a.elf/'elf-build.json').read_text())
 for name,record in builds['files'].items():
  f=a.elf/name
  if sha(f)!=record['sha256'] or f.stat().st_size!=record['size_bytes'] or f.read_bytes()[:6]!=b'\x7fELF\x01\x01':raise ValueError('ELF evidence mismatch')
 if set(builds['files'])!={'infra-node.elf','infra-station.elf','default.elf'}:raise ValueError('Incomplete ELF cohort')
 out.mkdir(parents=True,exist_ok=False);shutil.copytree(a.elf,out/'elf')
 policy,state=generate(out/'fixtures',sha(out/'elf/infra-node.elf'));policy['allocation']={'automatic':True};(out/'fixtures/node-policy.example.json').write_text(json.dumps(policy,indent=2)+'\n');shutil.copytree(out/'fixtures/bootfs',out/'bootfs')
 for f in (out/'elf').glob('*.elf'):shutil.copy2(f,out/'bootfs'/f.name)
 subprocess.run([sys.executable,str(a.spiffsgen),'0x510000',str(out/'bootfs'),str(out/'store.bin'),'--page-size','256','--block-size','4096','--obj-name-len','32','--meta-len','4','--use-magic','--use-magic-len'],check=True)
 sys.path.insert(0,str(r/'scripts'));import spiffs_image;import paired_bank_images as paired
 if spiffs_image.read_image((out/'store.bin').read_bytes(),0x510000)!={f.name:f.read_bytes() for f in (out/'bootfs').iterdir()}:raise ValueError('SPIFFS extraction mismatch')
 for name in ('firmware.bin','bootloader.bin','partitions.bin'):shutil.copy2(base/name,out/name)
 (out/'otadata.bin').write_bytes(paired.initial_otadata());(out/'bank_state.bin').write_bytes(paired.initial_bank_state(fw,(out/'store.bin').read_bytes(),app_data=True))
 paired.parse_record((out/'bank_state.bin').read_bytes()[:96],app_data=True)
 seed(r,policy,state,out/'appdata-unprovisioned.bin');shutil.copytree(base/'tools',out/'tools');(out/'scripts').mkdir()
 for name in ('provision_infrastructure.py','infrastructure_seed.c','install_infrastructure.py','configure_infrastructure.mjs','relay-enroll.mjs','repair_infrastructure.py','inspect_infrastructure_state.py','infrastructure_read.c'):shutil.copy2(ROOT/'scripts'/name,out/'scripts'/name)
 for name in ('AGENT-MODES.md','README.md','CONFORMANCE.md','WORKSTATION-MAINTENANCE.md','RELAY-CONTRACT.md','RECOVERY-QUALIFICATION.md','INCIDENT-2026-11-01.md'):shutil.copy2(ROOT/'profiles/esp32s3-infrastructure'/name,out/name)
 shutil.copytree(a.evidence,out/'evidence')
 manifest=copy.deepcopy(old);manifest['source']['system_apps']=commit(ROOT)
 manifest['source']['native_reuse']={'release':a.base_release_url,'archive_sha256':sha(a.base_archive),'manifest_sha256':sha(base/'flash-manifest.json'),'native_rebuilt':False}
 manifest['source']['spiffsgen_sha256']=sha(a.spiffsgen)
 manifest['qualification'].update(target_compiled=True,hardware_qualified='pending',cadence_qualified='pending',native_rebuilt=False)
 offsets={f['path']:{k:f[k] for k in ('offset','partition_bytes','first_install')} for f in old['files'] if 'offset' in f}
 manifest['files']=[]
 for f in sorted(out.rglob('*')):
  if not f.is_file():continue
  name=f.relative_to(out).as_posix();record={'path':name,'sha256':sha(f),'size_bytes':f.stat().st_size};record.update(offsets.get(name,{}));manifest['files'].append(record)
 (out/'flash-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (out/'SHA256SUMS').write_text(''.join(f['sha256']+'  '+f['path']+'\n' for f in manifest['files'])+sha(out/'flash-manifest.json')+'  flash-manifest.json\n')
 verify(out)
 with zipfile.ZipFile(str(out)+'.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for f in sorted(out.rglob('*')):
   if f.is_file():z.write(f,out.name+'/'+f.relative_to(out).as_posix())
 print(json.dumps({'bundle':str(out)+'.zip','sha256':sha(pathlib.Path(str(out)+'.zip')),'native_rebuilt':False}))
if __name__=='__main__':main()

