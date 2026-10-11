from pathlib import Path
import hashlib,json,zipfile,io
root=Path.cwd();out=root/'deliverables/split';out.mkdir(parents=True,exist_ok=True)
CAP=20*1024*1024;CHUNK=18*1024*1024;sha=lambda b:hashlib.sha256(b).hexdigest();records=[]
for target,version in [('X4','0.1.81'),('WATCH','1.1.2')]:
 source=root/'deliverables'/f'{target}-{version}-Shared-Files-Software-Qualified-Test-Bundle.zip'
 with zipfile.ZipFile(source) as z: original={n:z.read(n) for n in z.namelist()}
 test={k:v for k,v in original.items() if k.endswith('FULL-INITIAL-ERASES-DATA.bin') or k.startswith('SD/') or k in ['README.md','FILE-SHARING-INSTRUCTIONS.md']}
 # Test instructions name exactly what this small package contains.
 test['START-HERE.txt']=(f'{target} {version}: software-qualified initial test image.\nThe full BIN installs at offset0 and replaces internal settings/NVS/AppData and both banks. No device has been flashed or hardware-qualified.\n'+('Copy the contents of SD/ to the card root, preserving Apps/ and fonts/; do not delete your books or unrelated files.\n' if target=='X4' else 'This Watch baseline has no SD provider; no SD payload is required.\n')+'Read FILE-SHARING-INSTRUCTIONS.md. Sources, symbols and evidence are in separate companion archives. The original README describes the complete multi-archive set.\nNative Runtime0.2.8 / Files1.5.24. Hardware interoperability and live memory margins are untested.\n').encode()
 remaining={k:v for k,v in original.items() if k not in test};chunks={};reassembly=[]
 for name,b in remaining.items():
  if len(b)>CHUNK:
   names=[]
   for i,at in enumerate(range(0,len(b),CHUNK),1):
    part='REASSEMBLE/'+name+f'.part{i:03d}';chunks[part]=b[at:at+CHUNK];names.append(part)
   reassembly.append({'original_path':name,'bytes':len(b),'sha256':sha(b),'parts':names})
  else:chunks[name]=b
 def emit(label,files):
  path=out/f'{target}-{version}-{label}.zip'
  with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
   for n,b in sorted(files.items()):z.writestr(n,b)
  assert path.stat().st_size<CAP,(path,path.stat().st_size)
  with zipfile.ZipFile(path) as z:assert z.testzip() is None;assert {n:z.read(n) for n in z.namelist()}==files
  row={'target':target,'version':version,'file':str(path),'bytes':path.stat().st_size,'sha256':sha(path.read_bytes()),'files':len(files)};records.append(row);return row
 emit('TEST',test)
 groups=[];group={};size=0
 for name,b in sorted(chunks.items()):
  if group and size+len(b)>CHUNK:groups.append(group);group={};size=0
  group[name]=b;size+=len(b)
 if group:groups.append(group)
 for i,g in enumerate(groups,1):emit(f'SOURCE-SYMBOLS-EVIDENCE-{i:02d}',g)
 manifest={'original_bundle':source.name,'original_zip_sha256':sha(source.read_bytes()),'original_entries':{n:sha(b) for n,b in original.items()},'split_large_members':reassembly,'archives':[r for r in records if r['target']==target],'scope':'Each original ZIP member is preserved byte-for-byte, directly or via ordered binary concatenation. Original outer ZIP encoding is not reproduced.'}
 recovery={**{n:b for n,b in test.items() if n in original},**chunks}
 for r in reassembly:recovery[r['original_path']]=b''.join(recovery[n] for n in r['parts'])
 assert all(recovery[n]==b for n,b in original.items())
 script='''from pathlib import Path
import hashlib,json
root=Path.cwd();m=json.loads((root/"split-manifest.json").read_text())
for row in m["split_large_members"]:
 b=b"".join((root/p).read_bytes() for p in row["parts"])
 assert len(b)==row["bytes"] and hashlib.sha256(b).hexdigest()==row["sha256"]
 target=root/row["original_path"];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b)
for name,digest in m["original_entries"].items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest
print("All original bundle member hashes match.")
'''
 emit('INDEX-AND-REASSEMBLY',{'split-manifest.json':(json.dumps(manifest,indent=2)+'\n').encode(),'reassemble.py':script.encode(),'README.txt':b'For testing, only TEST.zip is required. Optional: extract ALL companion archives and TEST.zip into one new folder, then run python3 reassemble.py there to restore oversized source archives and verify every original member. No firmware execution or flashing occurs.\n'})
(out/'delivery-index.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(records,indent=2))
