#!/usr/bin/env python3
"""Offline recipe, immutable-bundle and additive-catalog negative controls."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

import build_native_app_store_qualification as build
import native_app_store_evidence as evidence
import publish_native_app_store as publish
import verify_native_app_store_catalog as catalog


def write(path,value):
    path.write_text(json.dumps(value,indent=2)+'\n')


class NativeAppStoreReleaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.source='a'*40
    def tearDown(self):
        self.tmp.cleanup()
    def arguments(self):
        repos={k:self.root/k for k in build.PINS}
        return build.compose_arguments((build.ROOT/build.RECIPE).read_bytes(),repos,self.root,
                                       self.root/'include',self.root/'profile',self.root/'runtime-sdk','1.4.0')
    def test_complete_recipe(self):
        args=self.arguments();build.validate_recipe(args,'1.4.0')
        self.assertEqual(len(build.CAPABILITIES),16)
        self.assertIn(('software.update.apps',1),build.CAPABILITIES)
        self.assertNotIn(('software.update.firmware',1),build.CAPABILITIES)
        self.assertNotIn(('app.catalog',1),build.CAPABILITIES)
        for selector in build.DEFINES:self.assertIn('-D'+selector,args)
        self.assertIn(str(build.ROOT/build.CATALOG_PROFILE),args)
        self.assertFalse(any('@' in x for x in args))
    def test_every_recipe_argument_required(self):
        args=self.arguments()
        for index in range(len(args)):
            with self.subTest(argument=args[index]),self.assertRaises(ValueError):
                build.validate_recipe(args[:index]+args[index+1:],'1.4.0')
    def test_extra_and_changed_flags_rejected(self):
        args=self.arguments()
        for flag in ('-DPORTABLE_UPDATE_FIRMWARE=1','-UPORTABLE_UPDATE_NATIVE_UI',
                     '-DPORTABLE_RUNTIME_APP_CATALOG','-DPORTABLE_UPDATE_VERSION="9.0.0"','-O0'):
            with self.subTest(flag=flag),self.assertRaises(ValueError):
                build.validate_recipe(args+[flag],'1.4.0')
        for selector in build.DEFINES:
            changed=[a+'=0' if a=='-D'+selector else a for a in args]
            with self.subTest(selector=selector),self.assertRaises(ValueError):
                build.validate_recipe(changed,'1.4.0')
    def test_changed_include_paths_rejected(self):
        args=self.arguments()
        for index,arg in enumerate(args):
            if arg.startswith('-I'):
                changed=args.copy();changed[index]='-I/tmp/unowned'
                with self.subTest(argument=arg),self.assertRaises(ValueError):
                    build.validate_recipe(changed,'1.4.0')
    def test_retained_recipe_bytes_required(self):
        with self.assertRaises(ValueError):
            build.compose_arguments(b'{}',{},self.root,self.root,self.root,self.root,'1.4.0')
    def test_legacy_manifests_unchanged(self):
        expected={'Apps/native/app_store.json':'086f3719624e9955fd1b85b24e86d6cfa4f61ecda743657f5fcf8e4c3c552db9',
                  'Apps/app_store.json':'04aa15b2a9fea2baf35ecfe0b558e6fa6c6a2be4177bd4fe240c048476c4c17e'}
        for name,digest in expected.items():self.assertEqual(evidence.sha(build.ROOT/name),digest)
    def test_missing_evidence_is_rejected(self):
        with self.assertRaises((ValueError,FileNotFoundError)):
            evidence.validate(self.root,self.source)
    def make_bundle(self):
        for name in evidence.DIRECT:(self.root/name).write_bytes((name+'\n').encode())
        write(self.root/'qualification.json',{'status':'PASS'})
        write(self.root/'catalog-binding.json',{'source_commit':self.source})
        publication={'tag':publish.TAG,'source_commit':self.source,
                     'elf_sha256':evidence.sha(self.root/'app_store.elf'),
                     'manifest_sha256':evidence.sha(self.root/'app_store.json'),
                     'admission_sha256':evidence.sha(self.root/'elf-memory-admission.json'),
                     'qualification_sha256':evidence.sha(self.root/'qualification.json'),
                     'catalog_binding_sha256':evidence.sha(self.root/'catalog-binding.json')}
        write(self.root/'publication.json',publication)
        with zipfile.ZipFile(self.root/(publish.TAG+'.zip'),'w') as z:
            for name in evidence.DIRECT:z.write(self.root/name,'qualification/app/'+name)
            for name in ('qualification.json','catalog-binding.json','publication.json'):z.write(self.root/name,name)
        (self.root/'SHA256SUMS').write_text(''.join(evidence.sha(self.root/n)+'  '+n+'\n' for n in sorted(publish.ASSETS-{'SHA256SUMS'})))
    def test_bundle_integrity(self):
        self.make_bundle();self.assertEqual(set(publish.verify_bundle(self.root,self.source)),publish.ASSETS)
        for name in sorted(publish.ASSETS):
            old=(self.root/name).read_bytes();(self.root/name).write_bytes(old+b'altered')
            with self.subTest(asset=name),self.assertRaises((ValueError,zipfile.BadZipFile)):
                publish.verify_bundle(self.root,self.source)
            (self.root/name).write_bytes(old)
        with self.assertRaises(ValueError):publish.verify_bundle(self.root,'b'*40)
    def catalog_inputs(self):
        before=self.root/'before.json';after=self.root/'after.json';binding=self.root/'binding.json';audit=self.root/'audit.json'
        expected={'release_tag':publish.TAG,'source_commit':self.source,'application':'app_store','version':'1.4.0',
                  'image':{'sha256':'b'*64},'manifest_sha256':'c'*64}
        write(binding,expected);write(before,{'entries':[{'id':'legacy','version':'1.1.5'}]})
        write(after,{'entries':[{'id':'legacy','version':'1.1.5'},expected]})
        write(audit,{'status':'PASS','all_public_assets_byte_verified':True,'tag':publish.TAG,'source_commit':self.source,
                     'public_asset_sha256':{'app_store.elf':'b'*64,'app_store.json':'c'*64,'catalog-binding.json':evidence.sha(binding)}})
        return before,after,binding,audit
    def test_catalog_only_adds_verified_binding(self):
        paths=self.catalog_inputs();self.assertEqual(catalog.verify(*paths)['preserved_entries'],1)
        original=json.loads(paths[1].read_text())
        variants=[{'entries':original['entries'][1:]}, {'entries':original['entries']+[{'id':'another'}]},
                  {'entries':original['entries']+[original['entries'][1]]},
                  {'entries':[{'id':'legacy','version':'changed'},original['entries'][1]]}]
        for variant in variants:
            write(paths[1],variant)
            with self.assertRaises(ValueError):catalog.verify(*paths)
        write(paths[1],original);audit=json.loads(paths[3].read_text());audit['public_asset_sha256']['app_store.elf']='d'*64;write(paths[3],audit)
        with self.assertRaises(ValueError):catalog.verify(*paths)
    def test_duplicate_keys_rejected(self):
        p=self.root/'duplicate.json';p.write_text('{"status":"PASS","status":"FAIL"}')
        with self.assertRaises(ValueError):evidence.read(p)
    def test_source_traversal_rejected(self):
        for name in ('../file','/tmp/file','a/../file'):
            with self.assertRaises(ValueError):evidence.relative(name)


if __name__=='__main__':unittest.main(verbosity=2)
