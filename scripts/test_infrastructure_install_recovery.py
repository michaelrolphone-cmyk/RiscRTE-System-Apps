#!/usr/bin/env python3
"""Regression tests for the production installer's OTA selection preflight."""
import pathlib,struct,subprocess,sys,unittest,zlib,tempfile
import install_infrastructure as installer

def snapshot(entries):
 image=bytearray(b'\xff'*installer.FLASH)
 for slot,(sequence,state) in enumerate(entries):
  seq=struct.pack('<I',sequence);entry=seq+b'\xff'*20+struct.pack('<II',state,zlib.crc32(seq,0xffffffff))
  image[0xff0000+slot*4096:0xff0020+slot*4096]=entry
 return image

class Selection(unittest.TestCase):
 def test_confirmed_bank_zero(self):self.assertEqual(installer.active_bank(snapshot([(1,2)])),(1,0,0))
 def test_confirmed_bank_one(self):self.assertEqual(installer.active_bank(snapshot([(1,2),(2,2)])),(2,1,1))
 def test_newest_unconfirmed_is_not_ignored(self):
  for state in (0,1):
   with self.subTest(state=state),self.assertRaises(AssertionError):installer.active_bank(snapshot([(1,2),(2,state)]))
 def test_equal_sequences_rejected(self):
  with self.assertRaises(AssertionError):installer.active_bank(snapshot([(1,2),(1,2)]))
 def test_failed_newer_bank_allows_confirmed_rollback(self):
  for state in (3,4):self.assertEqual(installer.active_bank(snapshot([(1,2),(2,state)])),(1,0,0))
 def test_unknown_state_rejected(self):
  with self.assertRaises(AssertionError):installer.active_bank(snapshot([(1,2),(2,5)]))
 def test_crc_failure_is_not_authority(self):
  image=snapshot([(1,2)]);image[0xff001c]^=1
  with self.assertRaises(AssertionError):installer.active_bank(image)
 def test_no_valid_selection(self):
  with self.assertRaises(AssertionError):installer.active_bank(snapshot([]))
 def test_optimized_python_cannot_disable_checks(self):
  result=subprocess.run([sys.executable,'-O',str(pathlib.Path(installer.__file__)),'--help'],capture_output=True,text=True)
  self.assertNotEqual(result.returncode,0);self.assertIn('do not run with -O',result.stderr)

class UpdatePlan(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.bundle=pathlib.Path(self.temp.name)
  self.firmware=b'RISC_PAIRED_STORE_ABI:2\0'+b'F'*512;self.store=b'S'*0x510000
  for name,data in [('firmware.bin',self.firmware),('store.bin',self.store),('partitions.bin',b'P'*3072),('bootloader.bin',b'B'*22640)]: (self.bundle/name).write_bytes(data)
  self.flash=snapshot([(1,2)]);self.flash[:22640]=b'B'*22640;self.flash[0x8000:0x8c00]=b'P'*3072
  self.flash[installer.FW[0]:installer.FW[0]+len(self.firmware)]=self.firmware;self.flash[installer.STORE[0]:installer.STORE[0]+len(self.store)]=self.store
  self.flash[0xff2000:0xff2060]=installer.record(0,self.firmware,self.store)
  self.flash[installer.DATA[0]:sum(installer.DATA)]=b'D'*installer.DATA[1]
 def test_each_interrupted_update_prefix_preserves_authority_and_data(self):
  writes=installer.update_plan(self.bundle,self.flash);self.assertEqual([x[0] for x in writes],[0x800000,0xae0000,0xff3000,0xff1000])
  for offset,data in writes[:-1]:
   self.flash[offset:offset+len(data)]=data;self.assertEqual(installer.active_bank(self.flash)[1],0);self.assertEqual(self.flash[installer.DATA[0]:sum(installer.DATA)],b'D'*installer.DATA[1])
  offset,data=writes[-1];self.flash[offset:offset+len(data)]=data
  with self.assertRaises(AssertionError):installer.active_bank(self.flash)
  # IDF confirms the candidate only after the application reports boot health.
  struct.pack_into('<I',self.flash,offset+24,2);self.assertEqual(installer.active_bank(self.flash)[1],1)
  self.assertEqual([x[0] for x in installer.update_plan(self.bundle,self.flash)],[0x10000,0x2f0000,0xff2000,0xff0000])
 def test_corrupt_pair_rejected(self):
  self.flash[installer.FW[0]+32]^=1
  with self.assertRaises(AssertionError):installer.update_plan(self.bundle,self.flash)
 def test_layout_change_rejected(self):
  self.flash[0x8000]^=1
  with self.assertRaises(AssertionError):installer.update_plan(self.bundle,self.flash)
 def test_recovery_selects_verified_pair_without_data_write(self):
  writes=installer.update_plan(self.bundle,self.flash,0);self.assertEqual(len(writes),1);offset,data=writes[0];self.flash[offset:offset+len(data)]=data;self.assertEqual(installer.active_bank(self.flash)[1],0)

if __name__=='__main__':unittest.main()
