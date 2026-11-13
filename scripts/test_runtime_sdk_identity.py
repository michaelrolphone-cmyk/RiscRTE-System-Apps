#!/usr/bin/env python3
"""SDK equality is not native source equality; reject dirty or incompatible trees."""
import json,os,subprocess,tempfile,unittest
from pathlib import Path
from runtime_sdk_identity import verify_runtime_sdk

class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.repo=Path(self.tmp.name)
        self.git('init','-q');self.git('config','user.name','SDK test');self.git('config','user.email','sdk@example.invalid')
        (self.repo/'sdk/app').mkdir(parents=True);(self.repo/'sdk/app/api.h').write_text('canonical\n');(self.repo/'LICENSE').write_text('license\n');self.commit();self.canonical=self.git('rev-parse','HEAD')
    def tearDown(self):self.tmp.cleanup()
    def git(self,*args):return subprocess.check_output(['git','-C',str(self.repo),*args],text=True,stderr=subprocess.PIPE).strip()
    def commit(self):self.git('add','.');self.git('commit','-qm','fixture')
    def verify(self):return verify_runtime_sdk(self.repo,self.canonical)
    def test_exact_and_native_only_successor(self):
        self.assertEqual(self.verify()['checkout_commit'],self.canonical)
        (self.repo/'native.c').write_text('native successor\n');self.commit();r=self.verify();self.assertNotEqual(r['checkout_commit'],r['canonical_commit']);self.assertTrue(r['checkout_clean']);self.assertEqual(set(r['exact_git_objects']),{'sdk','LICENSE'})
    def test_sdk_any_subtree_and_license_changes_rejected(self):
        for name in ['sdk/app/api.h','LICENSE']:
            self.git('reset','--hard',self.canonical);p=self.repo/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('changed\n');self.commit()
            with self.assertRaises(ValueError):self.verify()
    def test_additions_are_reported_without_claiming_tree_equality(self):
        for name in ['sdk/driver/new.h','sdk/hardware/extra.h']:
            p=self.repo/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('additive\n')
        self.commit();r=self.verify()
        self.assertNotIn('exact_git_objects',r)
        self.assertEqual(r['unchanged_canonical_sdk_files'],1)
        self.assertEqual(set(r['additive_sdk_files']),{'sdk/driver/new.h','sdk/hardware/extra.h'})
        self.assertEqual(set(r['canonical_git_objects']),{'sdk','LICENSE'})
    def test_removed_or_symlink_replacement_rejected(self):
        p=self.repo/'sdk/app/api.h';p.unlink();self.commit()
        with self.assertRaises(ValueError):self.verify()
        self.git('reset','--hard',self.canonical);p.unlink();p.symlink_to('../../LICENSE');self.commit()
        with self.assertRaises(ValueError):self.verify()
    def test_dirty_tracked_untracked_rejected(self):
        for name in ['sdk/app/api.h','untracked']:
            p=self.repo/name;p.write_text('dirty\n')
            with self.assertRaises(ValueError):self.verify()
            self.git('reset','--hard',self.canonical);self.git('clean','-fd')
    def test_unrelated_identical_sdk_rejected(self):
        self.git('checkout','--orphan','unrelated');self.git('add','.');self.git('commit','-qm','unrelated root')
        with self.assertRaises(ValueError):self.verify()
    def test_missing_canonical_rejected(self):
        with self.assertRaises(subprocess.CalledProcessError):verify_runtime_sdk(self.repo,'0'*40)
if __name__=='__main__':unittest.main()
