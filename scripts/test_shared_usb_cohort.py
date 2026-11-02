#!/usr/bin/env python3
"""Synthetic overlay invariants; actual bytes/packing/admission run separately."""
import copy,json,unittest
from assemble_shared_usb_cohort import overlay,OLD,MANIFEST,IMAGE,SD_IMAGE,grant,storage_record
from compose_shared_app_cohort import encode
from image_envelope import encode_image
ELF=b'\x7fELF'+b'x'*1024
COMMON=encode({'type':'application','id':'usb_sd_transfer','architecture':'xtensa-esp32s3',
 'version':'1.0.0','file_name':IMAGE,'entry':'app_main','requires':[{'capability':'ui.scene','api':1}],
 'optional':[{'capability':'usb.device.msc','api':1},{'capability':'alarm.service','api':2}]})
def fixture(lane):
 before={'default.elf':ELF,'default.json':encode({'type':'application','file_name':'default.elf'})}
 drivers=[]
 for name,cap,api in [('scene','ui.scene',1)]+([] if lane=='watch' else [('msc','usb.device.msc',1)])+([('alarm','alarm.service',2)] if lane=='x4' else []):
  path=name+'/manifest.json';drivers.append({'manifest':path})
  before[path]=encode({'type':'driver','file_name':'driver.elf','provides':[{'capability':cap,'api':api}]})
  before[name+'/driver.elf']=ELF
 rows=[{'manifest':'default.json','grants':[]}]
 sd={'other/data':b'keep'}
 if lane!='watch':
  rows.append(copy.deepcopy(OLD[lane]));before[MANIFEST]=COMMON
  if lane=='x4':before[IMAGE]=b'old'
  else:sd[SD_IMAGE]=b'old'
 before['boot.json']=encode({'default':'default.elf','drivers':drivers,'app_capabilities':rows,
                            'resident_shell':{'foreground':['usb_sd_transfer']},
                            'installation':{'namespace_range':[1,100]}})
 stored=dict(before)
 if lane=='watch':stored['default.elf']=encode_image(ELF)
 return before,stored,sd
class Overlay(unittest.TestCase):
 def test_all_lanes_preserve_unrelated(self):
  for lane in ('watch','wave','x4'):
   with self.subTest(lane=lane):
    b,s,d=fixture(lane);orig=copy.deepcopy((b,s,d))
    a,t,e=overlay(lane,b,s,d,ELF,COMMON)
    self.assertEqual((b,s,d),orig)
    self.assertEqual(a['default.elf'],b['default.elf'])
    self.assertEqual(t['default.elf'],s['default.elf'])
    self.assertEqual(e['other/data'],d['other/data'])
    self.assertEqual(a[MANIFEST],COMMON)
    boot=json.loads(a['boot.json'])
    self.assertEqual(boot['drivers'],json.loads(b['boot.json'])['drivers'])
    self.assertEqual(boot['resident_shell'],json.loads(b['boot.json'])['resident_shell'])
    r=[r for r in boot['app_capabilities'] if r['manifest']==MANIFEST][0]
    self.assertEqual(r['grants'],[grant('ui.scene')]+([] if lane=='watch' else [grant('usb.device.msc')])+([grant('alarm.service',2)] if lane=='x4' else []))
    self.assertEqual(e[SD_IMAGE] if lane=='wave' else a[IMAGE],ELF)
    if lane=='watch':self.assertIn('default.elf',storage_record(a,t)['images'])
 def test_authority_drift_rejected(self):
  b,s,d=fixture('x4');boot=json.loads(b['boot.json'])
  boot['app_capabilities'][-1]['grants'].append(grant('storage.volume',instance=9))
  b['boot.json']=s['boot.json']=encode(boot)
  with self.assertRaises(ValueError):overlay('x4',b,s,d,ELF,COMMON)
 def test_baseline_corruption_rejected(self):
  b,s,d=fixture('watch');s['default.elf']=b'bad'
  with self.assertRaises(ValueError):overlay('watch',b,s,d,ELF,COMMON)
 def test_missing_scene_rejected(self):
  b,s,d=fixture('wave');boot=json.loads(b['boot.json']);boot['drivers']=boot['drivers'][1:]
  b['boot.json']=s['boot.json']=encode(boot)
  with self.assertRaises(ValueError):overlay('wave',b,s,d,ELF,COMMON)
 def test_required_usb_rejected(self):
  b,s,d=fixture('watch');m=json.loads(COMMON);m['requires'].append({'capability':'usb.device.msc','api':1})
  with self.assertRaises(ValueError):overlay('watch',b,s,d,ELF,encode(m))
if __name__=='__main__':unittest.main()
