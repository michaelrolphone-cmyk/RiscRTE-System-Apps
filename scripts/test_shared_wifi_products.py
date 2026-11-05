#!/usr/bin/env python3
"""Synthetic regression controls, not product qualification."""
import copy,json,unittest
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
    def test_paths(self):
        for p in ('../x','/x','a/../b',r'a\\b','a//b',''):
            with self.assertRaises(ValueError):safe(p)
if __name__=='__main__':unittest.main()
