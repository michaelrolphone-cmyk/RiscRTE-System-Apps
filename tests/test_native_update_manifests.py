import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('portable_updates', ROOT / 'scripts/build_portable_updates.py')
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


class NativeUpdateManifestTests(unittest.TestCase):
    def test_native_versions_and_identity(self):
        for name in ('ota_update', 'app_store'):
            manifest = builder.native_update_manifest(name)
            self.assertEqual(name, manifest['id'])
            self.assertEqual(name + '.elf', manifest['file_name'])
            self.assertEqual('1.1.1', manifest['version'])

    def test_manifests_are_fresh_per_build(self):
        manifest = builder.native_update_manifest('ota_update')
        manifest['requires'].append({'capability': 'test', 'api': 1})
        self.assertFalse(any(r['capability'] == 'test' for r in builder.native_update_manifest('ota_update')['requires']))

    def test_unknown_app_rejected(self):
        with self.assertRaises(ValueError):
            builder.native_update_manifest('../settings')


if __name__ == '__main__':
    unittest.main()
