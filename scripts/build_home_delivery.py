#!/usr/bin/env python3
"""Describe an already-qualified, joined shared Home closure for the publisher."""
import argparse,hashlib,json,subprocess,tarfile,zipfile
from pathlib import Path
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--system',type=Path,required=True)
 a=p.parse_args();root=a.root.resolve()
 head=subprocess.check_output(['git','-C',str(a.system),'rev-parse','HEAD'],text=True).strip()
 subprocess.run(['git','-C',str(a.system),'diff','--exit-code','HEAD','--'],check=True)
 def ref(path):
  path=Path(path)
  if not path.is_file() or path.is_symlink():raise ValueError('Missing immutable closure file '+str(path))
  raw=path.read_bytes();return {'path':path.relative_to(root).as_posix(),'sha256':hashlib.sha256(raw).hexdigest(),'size':len(raw)}
 def files(directory):return [p for p in sorted(directory.rglob('*')) if p.is_file()]
 application={key:ref(root/'application'/name) for key,name in [('elf','default.elf'),('manifest','default.json'),('build','build.json')]}
 assembly=ref(root/'assembly/assembly.json')
 admissions={mode:ref(root/'admission'/mode/'receipt.json') for mode in ('normal','sanitized')}
 # Keep every owning source archive verbatim; collect actual license text,
 # not a fabricated license grant. Source and dependency provenance is zipped.
 source=root/'source';source.mkdir(exist_ok=True)
 archive=source/'System.tar.gz'
 if not archive.exists():
  raw=subprocess.check_output(['git','-C',str(a.system),'archive','--format=tar.gz','HEAD']);archive.write_bytes(raw)
 if (source/'System.head').read_text().strip()!=head:raise ValueError('Application owning source archive differs')
 commits={'system_apps':head}
 for marker in source.glob('*.head'):
  revision=marker.read_text().strip()
  if len(revision)!=40:raise ValueError('Invalid source revision '+str(marker))
  commits[marker.stem]=revision
 (root/'source-lock.json').write_text(json.dumps({'schema':1,'commits':commits},sort_keys=True,indent=2)+'\n')
 license_rows=[]
 archives=files(source)+[p for p in files(root/'dependencies') if p.name.endswith(('.tar.gz','.tar','.zip'))]
 def licensed(name):
  base=Path(name).name.lower()
  return base.startswith(('license','licence','copying','copyright','notice'))
 for path in archives:
  try:
   if path.name.endswith(('.tar.gz','.tar')):
    with tarfile.open(path,'r:*') as z:
     for member in z.getmembers():
      if member.isfile() and licensed(member.name):
       raw=z.extractfile(member).read();license_rows.append((path.relative_to(root).as_posix()+'/'+member.name,raw))
   elif path.name.endswith('.zip'):
    with zipfile.ZipFile(path) as z:
     for name in z.namelist():
      if not name.endswith('/') and licensed(name):license_rows.append((path.relative_to(root).as_posix()+'/'+name,z.read(name)))
  except (tarfile.TarError,zipfile.BadZipFile) as e:raise ValueError('Source archive malformed '+str(path)) from e
 if not license_rows:raise ValueError('Actual source license texts required')
 with zipfile.ZipFile(root/'licenses.zip','w',zipfile.ZIP_DEFLATED) as z:
  for i,(origin,raw) in enumerate(license_rows):
   z.writestr(str(i)+'-'+Path(origin).name,raw)
  z.writestr('origins.json',json.dumps([origin for origin,_ in license_rows],indent=2)+'\n')
 licenses=ref(root/'licenses.zip')
 dependencies=[]
 for family,receipt in [('built','dependencies.json'),('published','verified-public-assets.json'),('x4','receipt.json'),('native','artifact-lock.json')]:
  base=root/'dependencies'/family
  original=[p for p in files(base/'packages')]
  if family=='native':original += [p for p in files(base/'receipts') if p.suffix in ('.bin','.elf','.json')]
  family_sources=[p for p in files(base) if p.name.endswith(('.tar.gz','.tar','.zip'))]
  if family=='built':family_sources+=files(source)
  if not family_sources:raise ValueError('Owning dependency source closure missing '+family)
  dependencies.append({'receipt':ref(base/receipt),'artifacts':[ref(p) for p in original],
                       'sources':[ref(p) for p in family_sources],'licenses':[licenses]})
 # Compacted providers have their own exact semantic-equivalence record.
 dependencies.append({'receipt':ref(root/'packages/compaction.json'),
  'artifacts':[ref(p) for p in files(root/'packages') if p.name in ('driver.elf','manifest.json','contexts.elf','contexts.json','native.json','qualification.json','firmware.bin')],
  'sources':[ref(archive)],'licenses':[licenses]})
 product_assets={lane:{'store':ref(root/'assembly'/lane/'store.bin'),'sd':ref(root/'assembly'/lane/'sd-apps.zip')} for lane in ('watch','wave','x4')}
 for name in ('qualification.zip','provenance.zip'):
  if not (root/name).is_file():raise ValueError('Actual retained evidence archive required '+name)
 native_firmware={lane:{key:ref(root/'packages'/('native-'+lane)/filename) for key,filename in [('firmware','firmware.bin'),('qualification','qualification.json'),('descriptor','native.json')]} for lane in ('watch','wave','x4')}
 delivery={'native_firmware':native_firmware,'schema':1,'source_commit':head,'application':application,'assembly':assembly,'admissions':admissions,'dependencies':dependencies,'product_assets':product_assets,
  'source_archive':ref(archive),'source_lock':ref(root/'source-lock.json'),'provenance':ref(root/'provenance.zip'),'licenses':licenses,'qualification':ref(root/'qualification.zip')}
 if any(p.get('image_storage') for p in json.loads((root/'assembly/assembly.json').read_text())['products'].values()):
  guide=root/'UPGRADE.md';guide.write_bytes((a.system/'docs/shared-home/lossless-storage-upgrade.md').read_bytes())
  delivery['upgrade']={'guide':ref(guide),'receipt':ref(root/'upgrade/receipt.json'),
   'artifacts':{mode:ref(root/'upgrade'/(mode+'.zip')) for mode in ('normal','sanitized')}}
 (root/'home-delivery.json').write_text(json.dumps(delivery,sort_keys=True,indent=2)+'\n')
 print('Joined closure described; publisher must verify actual assembly and admission bindings')
if __name__=='__main__':main()
