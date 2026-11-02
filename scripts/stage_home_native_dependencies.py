#!/usr/bin/env python3
"""Stage owning full native firmware artifacts; never manufacture build success."""
import argparse,hashlib,json,re,zipfile
from pathlib import Path,PurePosixPath
def digest(raw):return hashlib.sha256(raw).hexdigest()
def safe(name):
 p=PurePosixPath(name)
 if p.is_absolute() or '..' in p.parts or '\\' in name:raise ValueError('Unsafe artifact path')
 return name
def record(raw):return {'bytes':len(raw),'sha256':digest(raw)}
def stage(archives,lock,output):
 if output.exists() or set(lock)!= {'watch','wave','x4'}:raise ValueError('Fresh complete native staging required')
 prepared={}
 for lane,item in lock.items():
  archive=archives/(lane+'.zip')
  with archive.open('rb') as f:
   if hashlib.file_digest(f,'sha256').hexdigest()!=item['artifact']['sha256']:raise ValueError('Owning native archive differs '+lane)
  with zipfile.ZipFile(archive) as z:
   names=z.namelist()
   if len(names)!=len(set(names)):raise ValueError('Duplicate native entries')
   for name in names:safe(name)
   def member(name):return z.read(safe(name))
   if lane=='x4':
    raw=member('artifacts/qualification.json');q=json.loads(raw)
    if q['sources']['product']['revision']!=item['source_revision'] or q['sources']['runtime']['revision']!=item['runtime_revision']:raise ValueError('X4 native sources differ')
    if q['factory_image'] is not False or q['hardware_tested'] is not False:raise ValueError('Unexpected X4 qualification scope')
    flashraw=member('artifacts/flasher_args.json')
    if record(flashraw)!=q['artifacts']['flasher_args']:raise ValueError('X4 flash descriptor differs')
    flash=json.loads(flashraw)
    paths={'firmware.bin':('firmware_bin',flash['app']['file']),'firmware.elf':('firmware_elf',str(PurePosixPath(flash['app']['file']).with_suffix('.elf'))),'bootloader.bin':('bootloader',flash['bootloader']['file']),'partitions.bin':('partitions',flash['partition-table']['file'])}
    files={}
    for dest,(key,path) in paths.items():
     data=member('artifacts/'+safe(path))
     if record(data)!=q['artifacts'][key]:raise ValueError('X4 native bytes differ '+key)
     files[dest]=data
    sources={name:member('metadata/'+name) for name in ('product-source.tar.gz','runtime-source.tar.gz','product-identity.txt','runtime-identity.txt','sdk-identity.txt','sdk-submodules.txt')}
   else:
    raw=member('candidate/qualification.json');q=json.loads(raw)
    for key in ('source_revision','runtime_revision','runtime_version'):
     if q[key]!=item[key]:raise ValueError('Native source/version differs '+lane+'/'+key)
    if q['hardware_tested'] is not False or q['product_composed'] is not False or q['device_accessed'] is not False:raise ValueError('Unexpected native qualification scope')
    expected={'Runtime.drivers_':48,'Runtime.providerStorage_':48,'Runtime.grants_':48,'Runtime.platforms_':32,'GraphV2.nodes_':48,'Invocation.appGrants_':16}
    if q['linked_capacity']!=expected or not q['capacity_memory'] or not q['native']:raise ValueError('Missing full native capacity/verification evidence')
    files={}
    for name in ('firmware.bin','firmware.elf','bootloader.bin','partitions.bin'):
     data=member('candidate/'+name)
     if record(data)!=q['artifacts'][name]:raise ValueError('Native bytes differ '+lane+'/'+name)
     files[name]=data
    sources={name:member('candidate/'+name) for name in ('source-lock.json','sources.zip','SHA256SUMS')}
   versions=set(re.findall(rb'RISC_RUNTIME_VERSION:([^\x00]+)\x00',files['firmware.bin']))
   if versions!={item['runtime_version'].encode()}:raise ValueError('Actual firmware Runtime marker differs '+lane)
   descriptor={key:item[key] for key in ('source_revision','runtime_revision','runtime_version')}
   descriptor.update(firmware={'path':'firmware.bin',**record(files['firmware.bin'])},qualification={'path':'qualification.json',**record(raw)},artifact=item['artifact'])
   prepared[lane]=(files,sources,raw,descriptor)
 output.mkdir(parents=True)
 for lane,(files,sources,raw,descriptor) in prepared.items():
  package=output/'packages'/('native-'+lane);package.mkdir(parents=True)
  (package/'firmware.bin').write_bytes(files['firmware.bin'])
  (package/'qualification.json').write_bytes(raw)
  (package/'native.json').write_text(json.dumps(descriptor,sort_keys=True,indent=2)+'\n')
  retained=output/'receipts'/lane;retained.mkdir(parents=True)
  for name,data in {**files,**sources,'qualification.json':raw}.items():(retained/name).write_bytes(data)
 (output/'artifact-lock.json').write_text(json.dumps(lock,sort_keys=True,indent=2)+'\n')
 print('All three exact full native dependencies staged; no product assembly or hardware claim')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('archives','lock','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();stage(a.archives,json.loads(a.lock.read_text()),a.output)
