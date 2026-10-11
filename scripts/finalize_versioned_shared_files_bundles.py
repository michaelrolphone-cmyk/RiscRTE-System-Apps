from pathlib import Path
import hashlib,json,shutil,subprocess,zipfile
root=Path.cwd();results=[]
sha=lambda b:hashlib.sha256(b).hexdigest()
for n,v in [('x4','0.1.81'),('watch','1.1.2')]:
 old=root/f'build/{n}-full-bundle';new=root/f'build/{n}-{v}-bundle'
 oldfiles={str(p.relative_to(old/'store')):sha(p.read_bytes()) for p in (old/'store').rglob('*') if p.is_file()}
 newfiles={str(p.relative_to(new/'store')):sha(p.read_bytes()) for p in (new/'store').rglob('*') if p.is_file()}
 assert oldfiles.keys()==newfiles.keys();changed=[p for p in oldfiles if oldfiles[p]!=newfiles[p]];assert changed==['cohort.json']
 for f in ['firmware.bin','bootloader.bin','partitions.bin','appdata.bin','otadata.bin']:assert (old/'components'/f).read_bytes()==(new/'components'/f).read_bytes()
 sdold={str(p.relative_to(old/'SD')):sha(p.read_bytes()) for p in (old/'SD').rglob('*') if p.is_file()};sdnew={str(p.relative_to(new/'SD')):sha(p.read_bytes()) for p in (new/'SD').rglob('*') if p.is_file()};assert sdold==sdnew
 previous=json.loads((old/'store/cohort.json').read_text());current=json.loads((new/'store/cohort.json').read_text());assert current['version']==v
 for k in previous:
  if k not in ['version','source_revision']:assert previous[k]==current[k]
 oldbin=next(old.glob('*FULL-INITIAL*.bin')).read_bytes();newbin=next(new.glob('*FULL-INITIAL*.bin')).read_bytes();assert oldbin[:0x2f0000]==newbin[:0x2f0000] and oldbin[0x800000:0xff2000]==newbin[0x800000:0xff2000] and oldbin[0xff4000:]==newbin[0xff4000:]
 for f in (old/'receipts').rglob('*'):
  dest=new/'receipts'/f.relative_to(old/'receipts')
  if f.is_file() and not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
 shutil.copy2(root/f'build/{n}-{v}-admitted/evidence.json',new/'receipts/final-admission.json')
 proof={'product_version':v,'store_only_changed':['cohort.json'],'cohort_only_changed':['version','source_revision'],'all_native_binaries_unchanged':True,'all_app_provider_elfs_unchanged':True,'all_sd_bytes_unchanged':True,'full_image_only_changed_regions':['bootfs0','bank_state'],'independent_image_verification':True,'full_image_sha256':sha(newbin),'previous_full_image_sha256':sha(oldbin)}
 (new/'receipts/version-correction-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
 (new/'sources').mkdir(exist_ok=True)
 sources={}
 for repo in ['runtime','system','drivers',n]:
  rev=subprocess.check_output(['git','-C',repo,'rev-parse','HEAD'],text=True).strip();branch=subprocess.check_output(['git','-C',repo,'branch','--show-current'],text=True).strip();remote=subprocess.check_output(['git','-C',repo,'rev-parse','refs/remotes/origin/'+branch],text=True).strip();assert remote==rev;assert not subprocess.check_output(['git','-C',repo,'status','--porcelain'],text=True).strip();sources[repo]={'commit':rev,'branch':branch,'published':True}
  if repo!='drivers':subprocess.run(['git','-C',repo,'archive','--format=zip','-o',str(new/'sources'/(repo+'-'+rev[:7]+'.zip')),rev],check=True)
 # Native X4 source remains independently pinned, despite product metadata successor.
 if n=='x4':shutil.copy2(old/'sources/X4-92f5a11.zip',new/'sources/X4-native-92f5a11.zip')
 sources['x4_native_source']='92f5a1122b7e7111ecbda604b1cada8352c6aa65' if n=='x4' else None
 sources['unchanged_store_baseline']='d38300bccdbf75d2d786bd3cbc87101fef1dabb7' if n=='x4' else '089868395268b696c05651c33816345bfe0425df'
 (new/'receipts/exact-sources.json').write_text(json.dumps(sources,indent=2)+'\n')
 shutil.copy2(root/'drivers/dist/ble-session-setup/build-record.json',new/'receipts/ble-provider-build.json')
 for f in ['NimBLE-SOURCE.json','NimBLE-LICENSE','NimBLE-NOTICE','TinyCrypt-LICENSE']:shutil.copy2(root/'drivers/dist/ble-session-setup'/f,new/'receipts'/f)
 use=(root/'system/docs/integration/FILE_SHARING_USE.md').read_text();(new/'FILE-SHARING-INSTRUCTIONS.md').write_text(use)
 (new/'README.md').write_text(f'''# {n.upper()} {v} — shared Files test bundle

Distinct product increment reserved in GitHub issue {'31' if n=='x4' else '65'}. Supersedes the ambiguously labeled shared Files test bundle; keeps all native/application/provider/SD bytes unchanged. Only product version/source identity, boot-store packing and bank-state hashes change. Full admission and independent image checks pass again.

The full initial image is `{n.upper()}-{v}-Shared-Files-FULL-INITIAL-ERASES-DATA.bin` (16MiB at offset0). Installing it erases internal settings/NVS/AppData and both banks. No device was flashed. This is not an OTA or preserving update.

See FILE-SHARING-INSTRUCTIONS.md for Wi-Fi sharing and optional BLE setup, including the absence of an on-device WebDAV client. `SD/` contains the matching X4 Apps/ and fonts/ tree, or a Watch note explaining that no SD payload/provider exists in this baseline. `store/`, `components/`, `debug/`, `sources/`, and `receipts/` preserve complete source/build/qualification custody. Older full-link evidence is historical; top-level receipts and version-correction-proof.json describe this exact bundle.

Runtime0.2.8 and Files1.5.24 are unchanged. X4 keeps Wi-Fi Settings1.1.26, MSC0.1.7, SD0.2.19, Reader0.1.8, Scene0.3.6, Daily Paper0.1.0 and panel0.1.12. Watch keeps Touch0.2.2 and Buttons0.1.22. Unfinished installer changes remain excluded. Hardware interoperability and live memory margins are untested. No master merge.

Full image SHA256: `{sha(newbin)}`.
''')
 files={str(p.relative_to(new)):sha(p.read_bytes()) for p in new.rglob('*') if p.is_file() and p.name!='SHA256SUMS'}
 (new/'SHA256SUMS').write_text(''.join(h+'  '+f+'\n' for f,h in sorted(files.items())))
 output=root/'deliverables'/f'{n.upper()}-{v}-Shared-Files-Software-Qualified-Test-Bundle.zip'
 with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for f in sorted(new.rglob('*')):
   if f.is_file():z.write(f,str(f.relative_to(new)))
 with zipfile.ZipFile(output) as z:
  assert z.testzip() is None;assert set(z.namelist())==set(files)|{'SHA256SUMS'};assert all(sha(z.read(f))==h for f,h in files.items())
 results.append({'target':n,'version':v,'path':str(output),'bytes':output.stat().st_size,'sha256':sha(output.read_bytes()),'sources':sources,'proof':proof})
(root/'deliverables/versioned-bundle-verification.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
