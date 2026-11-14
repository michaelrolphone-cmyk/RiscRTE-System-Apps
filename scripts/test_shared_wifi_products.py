#!/usr/bin/env python3
"""Synthetic regression controls, not product qualification."""
import copy,json,unittest,tempfile
from pathlib import Path
from test_shared_wifi_storage_authority import Authority
from unittest.mock import patch
from assemble_shared_wifi_products import assemble
from qualify_shared_wifi_products import preservation
from shared_wifi_products import sha,RUNTIME,elf_receipt
from shared_wifi_products import encode,grants,parity,safe,native_cohort
class Contracts(unittest.TestCase):
    def fixture(self):
        m={'id':'wifi_settings','file_name':'wifi_settings.elf','entry':'app_main','architecture':'xtensa-esp32s3','display_name':'Wi-Fi','icon':'solid:f1eb','requires':[{'capability':c,'api':1} for c in ('ui.scene','storage.key-value','net.wifi')]}
        row={'manifest':'wifi_settings.json','grants':[{'capability':'storage.key-value','api':1,'instance_id':6},{'capability':'storage.key-value','api':1,'instance_id':1},{'capability':'net.wifi','api':1,'instance_id':15}]}
        boot={'app_capabilities':[row],'drivers':[{'manifest':'scene/manifest.json'}],'resident_shell':{'foreground':['wifi_settings.elf']}}
        before={'boot.json':encode(boot),'scene/manifest.json':encode({'provides':[{'capability':'ui.scene','api':1}]})}
        return before,row,m
    def test_preserve_every_grant(self):
        before,row,m=self.fixture();result=grants(before,row,m)
        self.assertEqual(result[:-1],row['grants'])
        self.assertEqual(result[-1],{'capability':'ui.scene','api':1,'instance_id':0})
        for i in range(len(m['requires'])):
            bad=copy.deepcopy(m);bad['requires'].pop(i)
            with self.assertRaises(ValueError):grants(before,row,bad)
    def test_no_fabricated_atomic(self):
        before,row,m=self.fixture()
        m['requires'].append({'capability':'storage.key-value.atomic','api':1})
        with self.assertRaises(ValueError):grants(before,row,m)
    def test_ambiguous_scene(self):
        before,row,m=self.fixture();b=json.loads(before['boot.json']);b['drivers']*=2;before['boot.json']=encode(b)
        with self.assertRaises(ValueError):grants(before,row,m)
    def test_lifecycle_and_presentation(self):
        before,row,m=self.fixture();parity(before,row,m,m)
        for field in ('display_name','icon','file_name','id','entry','architecture'):
            bad=copy.deepcopy(m);bad[field]='different'
            with self.assertRaises(ValueError):parity(before,row,m,bad)
        b=json.loads(before['boot.json']);b.pop('resident_shell');before['boot.json']=encode(b)
        with self.assertRaises(ValueError):parity(before,row,m,m)
    def test_package_and_peer_tampering(self):
        for external in (False,True):
            with self.subTest(external=external),tempfile.TemporaryDirectory() as temp:
                root=Path(temp);before,row,m=self.fixture()
                oldelf=b'old app';elf=b'new app';oldscene=b'old scene';newscene=b'new scene'
                scene={'id':'scene-host','file_name':'driver.elf','driver_abi':2,'architecture':'xtensa-esp32s3','version':'old','provides':[{'capability':'ui.scene','api':1}],'requires':[]}
                before.update({'wifi_settings.json':encode(m),'wifi_settings.elf':oldelf,'scene/manifest.json':encode(scene),'scene/driver.elf':oldscene,'peer.bin':b'peer'})
                sd={'peer.bin':b'SD peer'}
                if external:
                    b=json.loads(before['boot.json'])
                    path='Drivers/scene-host/image-'+sha(oldscene)+'.elf'
                    b['drivers'][0]['image_source']={'path':path,'volume_instance':9,'internal_fallback':True}
                    before['boot.json']=encode(b);sd[path]=oldscene
                scene['version']='new';manifest=encode(scene)
                (root/'driver.elf').write_bytes(newscene);(root/'manifest.json').write_bytes(manifest)
                (root/'build.json').write_bytes(encode({'file':'driver.elf','size_bytes':len(newscene),'sha256':sha(newscene),'manifest_sha256':sha(manifest),'source_pins':{'runtime':RUNTIME,'system':'test-commit'},'working_tree_dirty':False,'optional_apis':{'connectivity':True}}))
                (root/'licenses').mkdir()
                for name in ('LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json','COMPONENT-SOURCES.json'):
                    source=Path(__file__).resolve().parents[1]/'Services/scene_host/fonts'/name
                    (root/'licenses'/name).write_bytes(source.read_bytes())
                with patch('shared_wifi_products.subprocess.check_output',return_value='test-commit'):
                    after,new_sd,*_=assemble(before,sd,elf,encode(m),root)
                    preservation(before,sd,after,new_sd,elf,encode(m),root)
                    self.assertEqual(after['scene/driver.elf'],newscene)
                    self.assertTrue(all(new_sd.get(n)==v for n,v in sd.items()))
                    for where in ('internal','sd','grant','provider-source'):
                        bad=copy.deepcopy(after);bad_sd=copy.deepcopy(new_sd)
                        if where=='internal':bad['peer.bin']=b'changed'
                        if where=='sd':bad_sd['peer.bin']=b'changed'
                        if where=='grant':
                            boot=json.loads(bad['boot.json']);boot['app_capabilities'][0]['grants'].pop(0);bad['boot.json']=encode(boot)
                        if where=='provider-source':bad['scene/driver.elf']=oldscene
                        with self.assertRaises(ValueError):preservation(before,sd,bad,bad_sd,elf,encode(m),root)
                    (root/'licenses/LICENSE-Orbitron.txt').write_bytes(b'wrong')
                    with self.assertRaises(ValueError):assemble(before,sd,elf,encode(m),root)
    def test_builder_receipt_contract(self):
        elf=b'target ELF';row={'file':'wifi_settings.elf','size_bytes':len(elf),'sha256':sha(elf)}
        elf_receipt(row,elf,'wifi_settings.elf')
        for field,value in (('file','driver.elf'),('size_bytes',len(elf)+1),('sha256','bad')):
            bad=dict(row);bad[field]=value
            with self.assertRaises(ValueError):elf_receipt(bad,elf,'wifi_settings.elf')
        bad=dict(row);bad['bytes']=bad.pop('size_bytes')
        with self.assertRaises(ValueError):elf_receipt(bad,elf,'wifi_settings.elf')

    def test_native_identity_requires_reviewed_lock(self):
        with tempfile.TemporaryDirectory() as temp:
            for lane in ('watch','wave','x4'):
                with self.assertRaisesRegex(ValueError,'Native product receipt absent'):
                    native_cohort({'assets':[]},Path(temp),lane)
    def native_fixture(self,root,lane='watch'):
        # Synthetic verifier fixture only; never installed/native build evidence.
        stores={f'product-{lane}-{s}':sha(s.encode()) for s in ('store.bin','sd-apps.zip')}
        blobs={k:('synthetic '+k).encode() for k in ('firmware','map','profile','settings_header','compile_commands','link_log')}
        blobs['elf']=b'\x7fELFsynthetic'
        r={'schema':1,'product':lane,'runtime_commit':RUNTIME,'product_commit':'1'*40,'working_tree_dirty':False,'scope':'linked-product-firmware','full_firmware_linked':True,'baseline_assets':stores}
        q={'product':lane,'runtime_commit':RUNTIME,'product_commit':r['product_commit'],'baseline_assets':stores,'firmware_sha256':sha(blobs['firmware']),'elf_sha256':sha(blobs['elf']),'capability_admission':'passed','persistent_storage_preservation':'passed','resource_budget':'passed'}
        lock={'assets':[{'name':n,'sha256':s} for n,s in stores.items()]}
        def seal():
            blobs['qualification']=encode(q)
            files={}
            for k,b in blobs.items():
                name=k+'.dat';(root/name).write_bytes(b)
                files[k]={'file':name,'size_bytes':len(b),'sha256':sha(b)}
            r['artifacts']=files
            data=encode(r);(root/'receipt.json').write_bytes(data)
            lock['native_cohorts']={lane:{'receipt':{'file':'receipt.json','size_bytes':len(data),'sha256':sha(data)},'runtime_commit':RUNTIME,'product_commit':'1'*40,'artifacts':copy.deepcopy(files)}}
        seal()
        return lock,r,q,blobs,seal
    def test_native_identity_rejects_source_only_and_mismatched_evidence(self):
        for lane in ('watch','wave','x4'):
            with tempfile.TemporaryDirectory() as temp:
                root=Path(temp);lock,r,q,blobs,seal=self.native_fixture(root,lane)
                self.assertEqual(native_cohort(lock,root,lane)['runtime_commit'],RUNTIME)
                original=copy.deepcopy(r)
                for field,value in (('runtime_commit','2'*40),('product_commit','2'*40),('product','other'),('working_tree_dirty',True),('scope','host-only'),('full_firmware_linked',False),('baseline_assets',{})):
                    r.clear();r.update(copy.deepcopy(original));r[field]=value;seal()
                    with self.subTest(lane=lane,field=field),self.assertRaises(ValueError):native_cohort(lock,root,lane)
                r.clear();r.update(original);seal()
                for field in ('capability_admission','persistent_storage_preservation','resource_budget'):
                    q[field]='not measured';seal()
                    with self.assertRaises(ValueError):native_cohort(lock,root,lane)
                    q[field]='passed'
                q['firmware_sha256']='0'*64;seal()
                with self.assertRaises(ValueError):native_cohort(lock,root,lane)
    def test_native_identity_binds_every_evidence_byte(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);lock,r,q,blobs,seal=self.native_fixture(root)
            for name in ('receipt.json',*[k+'.dat' for k in blobs]):
                seal();(root/name).write_bytes(b'changed')
                with self.subTest(name=name),self.assertRaises(ValueError):native_cohort(lock,root,'watch')
            seal();lock['native_cohorts']['watch']['receipt']['file']='../outside'
            with self.assertRaises(ValueError):native_cohort(lock,root,'watch')

    def test_paths(self):
        for p in ('../x','/x','a/../b',r'a\\b','a//b',''):
            with self.assertRaises(ValueError):safe(p)
if __name__=='__main__':unittest.main()
