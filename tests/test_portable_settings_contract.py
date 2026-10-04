import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PortableSettingsContract(unittest.TestCase):
    def test_rtc_declaration_is_exact_pinned_excerpt(self):
        record=json.loads((ROOT/'lib/PortableApps/RTC_PROVENANCE.json').read_text())
        header=(ROOT/record['consumer_header']).read_text()
        block=header.split('/* BEGIN VERBATIM RTC DECLARATION */\n')[1].split('\n/* END VERBATIM RTC DECLARATION */')[0]
        self.assertEqual(hashlib.sha256(block.encode()).hexdigest(),record['verbatim_block_sha256'])
        self.assertEqual(record['api'],2)
        self.assertEqual(record['capability'],'rtc.clock')
        self.assertIn('_Static_assert(sizeof(twatch_rtc_api_v1) == 28',header)
        self.assertIn('offsetof(twatch_rtc_api_v1, alarm_pending) == 24',header)

    def test_shared_time_helper_and_fonts_remain_pinned(self):
        record=json.loads((ROOT/'lib/PortableApps/time/SOURCES.json').read_text())
        for item in record['files']:
            self.assertEqual(hashlib.sha256((ROOT/item['path']).read_bytes()).hexdigest(),item['sha256'])
        fonts=ROOT/'lib/PortableApps/settings_fonts'
        record=json.loads((fonts/'SOURCES.json').read_text())
        self.assertEqual(hashlib.sha256((fonts/'text.inc').read_bytes()).hexdigest(),record['text.inc_sha256'])
        for family in ['Orbitron','Rajdhani']:
            self.assertIn('SIL OPEN FONT LICENSE',(fonts/('LICENSE-'+family+'.txt')).read_text())

    def test_original_settings_navigation_is_reused(self):
        data=(ROOT/'Apps/settings.c').read_bytes()
        blob=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        self.assertEqual(blob,'0e2b5fb00c877837789e503e91a4a220f10c83e1')

    def test_settings_development_version_matches_inventory(self):
        manifest=json.loads((ROOT/'Apps/settings.json').read_text())
        inventory=json.loads((ROOT/'system-apps-manifest.json').read_text())
        item=next(x for x in inventory['apps'] if x['id']=='settings')
        self.assertEqual(manifest['version'],item['version'])
        self.assertGreater(tuple(map(int,manifest['version'].split('.'))),(1,0,1))

if __name__=='__main__':unittest.main()
