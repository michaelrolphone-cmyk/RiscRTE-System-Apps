import importlib.util
from pathlib import Path
import unittest
import argparse
import tempfile
import hashlib

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
            self.assertEqual('1.1.5', manifest['version'])

    def test_manifests_are_fresh_per_build(self):
        manifest = builder.native_update_manifest('ota_update')
        manifest['requires'].append({'capability': 'test', 'api': 1})
        self.assertFalse(any(r['capability'] == 'test' for r in builder.native_update_manifest('ota_update')['requires']))

    def test_explicit_provider_policy_preserves_app_builds(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / 'Product.h'
            header.write_text('// selected provider policy\n')
            args = argparse.Namespace(product_policy_header=header, services_only=True, catalog_url=None)
            flags, receipt = builder.product_policy(args, argparse.ArgumentParser())
            self.assertEqual(['-DUPDATE_PRODUCT_POLICY_HEADER="'+str(header.resolve())+'"'], flags)
            self.assertEqual(hashlib.sha256(header.read_bytes()).hexdigest(), receipt['header_sha256'])
            args.services_only = False
            with self.assertRaises(SystemExit):
                builder.product_policy(args, argparse.ArgumentParser())
            args.services_only = True
            args.catalog_url = 'https://example.org/feed'
            with self.assertRaises(SystemExit):
                builder.product_policy(args, argparse.ArgumentParser())
            args.catalog_url = None
            header.unlink()
            with self.assertRaises(SystemExit):
                builder.product_policy(args, argparse.ArgumentParser())

    def test_unknown_app_rejected(self):
        with self.assertRaises(ValueError):
            builder.native_update_manifest('../settings')


if __name__ == '__main__':
    unittest.main()
