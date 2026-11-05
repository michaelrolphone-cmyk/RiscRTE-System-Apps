#!/usr/bin/env python3
"""Synthetic regression controls, not product qualification."""
import copy,json,unittest,tempfile
from pathlib import Path
from unittest.mock import patch
from assemble_shared_wifi_products import assemble
from qualify_shared_wifi_products import preservation
from shared_wifi_products import sha,RUNTIME
from shared_wifi_products import encode,grants,parity,safe
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
                (root/'build.json').write_bytes(encode({'bytes':len(newscene),'sha256':sha(newscene),'manifest_sha256':sha(manifest),'source_pins':{'runtime':RUNTIME,'system':'test-commit'},'working_tree_dirty':False,'optional_apis':{'connectivity':True}}))
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
    def test_paths(self):
        for p in ('../x','/x','a/../b',r'a\\b','a//b',''):
            with self.assertRaises(ValueError):safe(p)
if __name__=='__main__':unittest.main()
