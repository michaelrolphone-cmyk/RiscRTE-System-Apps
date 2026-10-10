import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PortableWifiContract(unittest.TestCase):
    def test_reader_branch_is_unchanged(self):
        data = (ROOT/'Apps/wifi_settings.c').read_bytes()
        legacy = data.split(b'#else\n', 1)[1].split(b'\n#endif /* Reader')[0]
        blob = hashlib.sha1(b'blob '+str(len(legacy)).encode()+b'\0'+legacy).hexdigest()
        self.assertEqual(blob, '33fd4a4045daecce210e1e347bedd599e146245d')

    def test_manifest_and_inventory_are_fresh(self):
        manifest = json.loads((ROOT/'Apps/wifi_settings.json').read_text())
        inventory = json.loads((ROOT/'system-apps-manifest.json').read_text())
        app = next(x for x in inventory['apps'] if x['id']=='wifi_settings')
        self.assertEqual(manifest['version'], '1.1.6')
        self.assertEqual(manifest['version'], app['version'])
        self.assertIsNone(app['additional_sources'][0]['upstream_blob'])

    def test_standard_watch_keyboard_shared_by_both_views(self):
        header = (ROOT/'lib/PortableApps/include/PortableWifiView.h').read_text()
        controller = (ROOT/'Apps/wifi_settings_portable.inc').read_text()
        view = (ROOT/'lib/PortableApps/src/wifi_nova.inc').read_text()
        self.assertIn('PWK_COUNT', header)
        self.assertIn('PWK_CHARACTERS', header)
        self.assertIn('portable_watch_key_character(key_page,key)', controller)
        self.assertIn('PWK_INITIAL_PAGE', controller)
        self.assertIn('portable_watch_key_hit(x,y)', view)
        self.assertIn('portable_watch_key_bounds(i,&r)', view)
        for text in (header, controller, view):
            self.assertNotIn('PortableNovaKeyboard', text)
            self.assertNotIn('portable_nova_key_character', text)

    def test_no_firmware_network_ui_or_eager_return(self):
        source = (ROOT/'Apps/wifi_settings_portable.inc').read_text()
        for name in ('t5_network_get_api', 't5_system_ui_get_api', 'wifi_request', 'wifi_take_result'):
            self.assertNotIn(name, source)
        self.assertIn('#error "Wi-Fi owns nested Back;', source)
        self.assertIn('WIFI_RETURN_APP', source)
        self.assertIn('WIFI_MANAGEMENT_V1_SIZE', source)
        self.assertIn('portable_wifi_credentials_clear', source)

    def test_sleep_guard_precedes_native_touch_teardown(self):
        source = (ROOT/'lib/PortableApps/src/adapter.c').read_text()
        sleep = source.split('static bool idle_sleep(void)', 1)[1].split('static bool poll_input', 1)[0]
        self.assertLess(sleep.index('portable_wifi_suspend'), sleep.index('portable_touch_close'))
        self.assertLess(sleep.index('if(status==-2)'), sleep.index('portable_wifi_resume'))
        fini = source.split('void app_module_fini(void)', 1)[1]
        self.assertLess(fini.index('if(native_sleep_retained)return'), fini.index('portable_wifi_close'))

if __name__ == '__main__':
    unittest.main()
