"""Bounds, source custody and packaging tests for the new external GUI app."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'Apps/paper_space'
spec = importlib.util.spec_from_file_location('build_paper_space', ROOT / 'scripts/build_paper_space.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class PaperSpaceTests(unittest.TestCase):
    def check_catalog(self, value):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'catalog.json'
            path.write_text(json.dumps(value))
            return builder.catalog_items(path)

    def test_example_order_and_empty_catalog(self):
        self.assertEqual([x['label'] for x in builder.catalog_items(APP / 'catalog.example.json')], ['Apps', 'Settings'])
        self.assertEqual(self.check_catalog([]), [])

    def test_normalized_catalog_paths(self):
        for path in ['../app.elf', '/app.elf', 'a//app.elf', '.hidden/app.elf', 'a/../app.elf',
                     'a\\app.elf', 'default.elf', 'app.ELF', 'a..b.elf', 'a/app.elf/', 'a/'+'b'*193+'.elf']:
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.check_catalog([{'label': 'Apps', 'path': path}])
        self.check_catalog([{'label': 'Apps', 'path': 'apps/springboard.elf'}])

    def test_catalog_count_labels_and_duplicates(self):
        valid = {'label': 'Apps', 'path': 'apps.elf'}
        invalid = [[valid]*9, [valid, valid], [{'label': '', 'path': 'apps.elf'}],
                   [{'label': 'x'*49, 'path': 'apps.elf'}], [{'label': '\n', 'path': 'apps.elf'}],
                   [{'label': 'é', 'path': 'apps.elf'}], [{'label': 'Apps', 'path': 'apps.elf', 'grant': True}], {}]
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.check_catalog(value)

    def test_c_strings_disable_trigraphs(self):
        self.assertEqual(builder.c_string('Do ??/'), '"Do \\?\\?/"')

    def test_canonical_sdk_custody(self):
        provenance = json.loads((APP / 'SOURCES.json').read_text())
        for name, record in provenance['inputs'].items():
            if name.startswith('sdk/'):
                self.assertEqual(hashlib.sha256((APP / name).read_bytes()).hexdigest(), record['sha256'])
                self.assertEqual(record['mode'], 'verbatim')
        self.assertEqual(hashlib.sha256((APP / 'fonts/PaperSpaceText12.h').read_bytes()).hexdigest(),
                         provenance['font_subset']['sha256'])

    def test_manifest_is_runtime_app_not_firmware_activity(self):
        manifest = json.loads((APP / 'manifest.json').read_text())
        self.assertEqual(manifest['id'], 'paper-space')
        self.assertEqual(manifest['version'], '1.0.0')
        self.assertEqual(manifest['entry'], 'app_main')
        self.assertEqual(manifest['file_name'], 'default.elf')
        self.assertEqual(manifest['requires'], [{'capability': 'display.output', 'api': 1},
                                               {'capability': 'input.navigation', 'api': 1}])


if __name__ == '__main__':
    unittest.main()
