import argparse
import unittest
import tempfile
from types import SimpleNamespace
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
import portable_file_sharing_build as sharing
class SharingProfile(unittest.TestCase):
 def options(self,argv):
  p=argparse.ArgumentParser();sharing.options(p);a=p.parse_args(argv);sharing.validate(a,p);return a
 def test_default_is_empty(self):
  a=self.options([]);self.assertEqual(sharing.configure(a,None,None,None,None),([],[],[]))
  needs=[{"capability":"display.output","api":1}];sharing.requirements(a,needs);self.assertEqual(len(needs),1)
 def test_explicit_selection(self):
  a=self.options(["--webdav-sharing","--sharing-export-instance","0","--sharing-tcp-instance","0","--sharing-entropy-instance","0"]);needs=[];sharing.requirements(a,needs);sharing.requirements(a,needs);self.assertEqual(len(needs),5)
 def test_explicit_ble_keeps_http_optional(self):
  argv=["--webdav-sharing","--sharing-export-instance","0","--sharing-tcp-instance","0","--sharing-entropy-instance","0"]
  a=self.options(argv+["--sharing-setup-instance","0"]);needs=[];sharing.requirements(a,needs)
  self.assertEqual(len(needs),7);self.assertEqual(sharing.selected_version(a),'1.5.30')
  self.assertEqual(sharing.selected_version(self.options(argv)),'1.5.30')
  for invalid in (["--sharing-setup-instance","0"],argv+["--sharing-setup-instance","-1"],argv+["--sharing-setup-instance","35"]):
   with self.assertRaises(SystemExit):self.options(invalid)
 def test_ble_power_authority_is_explicit(self):
  a=self.options(["--webdav-sharing","--sharing-export-instance","0","--sharing-tcp-instance","0","--sharing-entropy-instance","0","--sharing-setup-instance","0"])
  a.storage_capability='storage.app-data.export';a.storage_instance=0;a.secondary_storage_instance=None
  needs=[];sharing.requirements(a,needs);sharing.requirements(a,needs)
  self.assertEqual(needs.count({'capability':'bluetooth.hci','api':1}),1)
  record={'source_sha256':{}}
  with tempfile.TemporaryDirectory() as d:sharing.record(a,ROOT,Path(d),{'requires':needs},record)
  self.assertEqual(record['grant_bindings']['bluetooth.hci'],[16])
  self.assertIn({'capability':'bluetooth.hci','api':1,'instance_id':16},record['required_grants'])
 def test_x4_volume_only_stays_within_sixteen_grants(self):
  a=self.options(["--webdav-sharing","--sharing-export-instance","0","--sharing-tcp-instance","0","--sharing-entropy-instance","0","--sharing-setup-instance","0"])
  a.storage_capability='storage.volume';a.storage_instance=9;a.secondary_storage_instance=None
  base=[('display.output',1,3),('input.touch.raw',1,4),('storage.volume',1,9),
        ('file.open',1,0),('input.navigation',1,6),('storage.key-value',1,1),
        ('board.battery',1,7),('alarm.service',2,0),('runtime.realtime',1,0),
        ('telemetry.broadcast',1,0)]
  needs=[{'capability':n,'api':v} for n,v,i in base]
  sharing.requirements(a,needs);sharing.requirements(a,needs)
  record={'source_sha256':{},'grant_bindings':{n:[i] for n,v,i in base}}
  with tempfile.TemporaryDirectory() as d:sharing.record(a,ROOT,Path(d),{'requires':needs},record)
  self.assertEqual(record['grant_count'],16)
  self.assertFalse(record['file_sharing']['app_data_export_required'])
  self.assertFalse(any(g['capability']=='storage.app-data.export' for g in record['required_grants']))
  self.assertIn({'capability':'storage.volume','api':1,'instance_id':9},record['required_grants'])
  self.assertIn({'capability':'bluetooth.hci','api':1,'instance_id':16},record['required_grants'])
 def test_mixed_volume_export_keeps_export_authority(self):
  a=SimpleNamespace(storage_capability='storage.volume',secondary_storage_instance=0,secondary_storage_capability='storage.app-data.export')
  self.assertTrue(sharing.export_required(a))
  a.secondary_storage_capability='storage.volume'
  self.assertFalse(sharing.export_required(a))
  a.storage_capability='storage.app-data.export'
  self.assertTrue(sharing.export_required(a))
 def test_reject_incomplete_or_ambiguous(self):
  for argv in (["--webdav-sharing"],["--sharing-export-instance","0"],["--webdav-sharing","--sharing-export-instance","32","--sharing-tcp-instance","0","--sharing-entropy-instance","0"],["--webdav-sharing","--sharing-export-instance","0","--sharing-tcp-instance","33","--sharing-entropy-instance","0"]):
   with self.assertRaises(SystemExit):self.options(argv)
if __name__=="__main__":unittest.main()
