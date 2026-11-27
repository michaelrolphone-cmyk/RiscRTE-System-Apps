#!/usr/bin/env python3
import importlib.util,pathlib,subprocess,tempfile,unittest
from unittest.mock import patch
p=pathlib.Path(__file__).with_name('native_scene_provenance.py');s=importlib.util.spec_from_file_location('proof',p);proof=importlib.util.module_from_spec(s);s.loader.exec_module(proof)
class Provenance(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name);self.file=self.root/'lib/PortableApps/src/adapter.c';self.file.parent.mkdir(parents=True);self.file.write_text('int unchanged;\n')
  self.git('init','-q');self.git('add','.');self.git('-c','user.name=Fixture','-c','user.email=fixture@example.invalid','commit','-qm','fixture');self.rev=self.git('rev-parse','HEAD').strip().decode()
 def tearDown(self):self.temp.cleanup()
 def git(self,*args):return subprocess.check_output(['git','-C',str(self.root),*args],stderr=subprocess.STDOUT)
 def test_exact_role(self):
  row=proof.bind(self.root,self.rev);self.assertEqual(row['commit'],self.rev);self.assertIn('lib/PortableApps/src/adapter.c',row['sources'])
 def test_wrong_revision(self):
  with self.assertRaises(ValueError):proof.bind(self.root,'0'*40)
 def test_dirty_source(self):
  self.file.write_text('int changed;\n')
  with self.assertRaises(ValueError):proof.bind(self.root,self.rev)
 def test_missing_source(self):
  self.file.unlink()
  with self.assertRaises(ValueError):proof.bind(self.root,self.rev)
 def test_untracked_source(self):
  self.file.with_name('extra.h').write_text('int extra;\n')
  with self.assertRaises(ValueError):proof.bind(self.root,self.rev)
 def test_assume_unchanged_cannot_hide_changed_bytes(self):
  self.git('update-index','--assume-unchanged','lib/PortableApps/src/adapter.c');self.file.write_text('int hidden_change;\n')
  with self.assertRaises(ValueError):proof.bind(self.root,self.rev)
 def test_historical_roles_are_fixed_and_distinct(self):
  with patch.object(proof,'bind',side_effect=lambda root,rev:rev) as bind:
   result=proof.historical('scene','before');self.assertEqual(result,{'scene':proof.SCENE,'before_scene':proof.BEFORE});self.assertNotEqual(proof.SCENE,proof.BEFORE)
if __name__=='__main__':unittest.main()
