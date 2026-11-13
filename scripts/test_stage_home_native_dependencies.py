#!/usr/bin/env python3
"""Native staging archive tests, not native firmware qualification."""
import json,tempfile,unittest,zipfile
from pathlib import Path
import stage_home_native_dependencies as s
REV='1'*40;RUNTIME='2'*40
class NativeStaging(unittest.TestCase):
 def fixture(self,root,mutate=None):
  lock={}
  for lane in ('watch','wave','x4'):
   marker=b'fake firmware RISC_RUNTIME_VERSION:0.4.19\0'
   artifacts={'firmware.bin':marker,'firmware.elf':b'elf','bootloader.bin':b'boot','partitions.bin':b'part'}
   item={'source_revision':REV,'runtime_revision':RUNTIME,'runtime_version':'0.4.19','artifact':{'repository':'fixture','id':1}}
   if lane=='x4':
    flash={'app':{'file':'image.bin'},'bootloader':{'file':'boot/boot.bin'},'partition-table':{'file':'parts.bin'}}
    raw=json.dumps(flash).encode();data={'artifacts/flasher_args.json':raw}
    q={'sources':{'product':{'revision':REV},'runtime':{'revision':RUNTIME}},'factory_image':False,'hardware_tested':False,'artifacts':{'flasher_args':s.record(raw)}}
    for dest,key,path in [('firmware.bin','firmware_bin','image.bin'),('firmware.elf','firmware_elf','image.elf'),('bootloader.bin','bootloader','boot/boot.bin'),('partitions.bin','partitions','parts.bin')]:
     data['artifacts/'+path]=artifacts[dest];q['artifacts'][key]=s.record(artifacts[dest])
    for name in ('product-source.tar.gz','runtime-source.tar.gz','product-identity.txt','runtime-identity.txt','sdk-identity.txt','sdk-submodules.txt'):data['metadata/'+name]=b'fixture'
    receipt='artifacts/qualification.json'
   else:
    q={k:item[k] for k in ('source_revision','runtime_revision','runtime_version')}
    q.update(hardware_tested=False,product_composed=False,device_accessed=False,
     linked_capacity={'Runtime.drivers_':48,'Runtime.providerStorage_':48,'Runtime.grants_':48,'Runtime.platforms_':32,'GraphV2.nodes_':48,'Invocation.appGrants_':16},
     capacity_memory={'fixture':True},native={'fixture':True},artifacts={n:s.record(b) for n,b in artifacts.items()})
    data={'candidate/'+n:b for n,b in artifacts.items()}
    for name in ('source-lock.json','sources.zip','SHA256SUMS'):data['candidate/'+name]=b'fixture'
    receipt='candidate/qualification.json'
   if mutate:mutate(lane,q,data)
   data[receipt]=json.dumps(q).encode()
   path=root/(lane+'.zip')
   with zipfile.ZipFile(path,'w') as z:
    for n,b in data.items():z.writestr(n,b)
   item['artifact']['sha256']=s.digest(path.read_bytes());lock[lane]=item
  return lock
 def test_three_actual_archive_layouts(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);lock=self.fixture(root);s.stage(root,lock,root/'out')
   for lane in lock:
    package=root/'out/packages'/('native-'+lane)
    d=json.loads((package/'native.json').read_text())
    self.assertEqual(d['firmware']['sha256'],s.digest((package/'firmware.bin').read_bytes()))
 def test_stale_or_missing_proofs_refused(self):
  def bad_source(l,q,d):
   if l=='watch':q['source_revision']='3'*40
  def bad_capacity(l,q,d):
   if l=='wave':q['linked_capacity']['Invocation.appGrants_']=32
  def bad_firmware(l,q,d):
   if l=='x4':d['artifacts/image.bin']=b'changed'
  def bad_path(l,q,d):d['../outside']=b'no'
  for mutation in (bad_source,bad_capacity,bad_firmware,bad_path):
   with self.subTest(mutation=mutation),tempfile.TemporaryDirectory() as t:
    root=Path(t);lock=self.fixture(root,mutation)
    with self.assertRaises(ValueError):s.stage(root,lock,root/'out')
    self.assertFalse((root/'out').exists())
 def test_wrong_archive_refused(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);lock=self.fixture(root);lock['watch']['artifact']['sha256']='0'*64
   with self.assertRaises(ValueError):s.stage(root,lock,root/'out')
if __name__=='__main__':unittest.main()
