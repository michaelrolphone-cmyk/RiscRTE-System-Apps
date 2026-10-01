import json
import pathlib
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'scripts'))
from app_manifest import validate_manifest
from check_baseline import ROOT, audit, check_sdk, classify, git_blob
from native_app_symbols import validate_imports
from package_integrity import stamp_app_manifest


class PipelineTests(unittest.TestCase):
    def test_sdk_snapshot_and_inventory(self):
        check_sdk()
        rows = audit()['files']
        self.assertEqual(len(rows), 36)
        self.assertTrue(all(row['state'] == 'unchanged' for row in rows))

    def test_three_way_conflict_preservation(self):
        for base, local, upstream, state in [('a','a','a','unchanged'), ('a','b','b','converged'),
                                            ('a','a','b','upstream-only'), ('a','b','a','external-only'),
                                            ('a','b','c','conflict')]:
            self.assertEqual(classify(base, local, upstream), state)

    def test_git_blob_identity(self):
        self.assertEqual(git_blob(b''), 'e69de29bb2d1d6434b8b29ae775ad8c2e48c5391')

    def test_all_app_manifests(self):
        for app in json.loads((ROOT / 'system-apps-manifest.json').read_text())['apps']:
            source = ROOT / app['source_path']
            validate_manifest(source, pathlib.Path(app['file_name']))
            self.assertEqual(json.loads(source.with_suffix('.json').read_text())['version'], app['version'])

    def test_unknown_import_rejected(self):
        with self.assertRaises(ValueError):
            validate_imports(' 1: 00000000 0 FUNC GLOBAL DEFAULT UND privileged_unsafe', {'printf'})
        self.assertEqual(validate_imports(' 1: 00000000 0 FUNC GLOBAL DEFAULT UND printf', {'printf'}), {'printf'})

    def test_stamped_artifact_cannot_silently_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = pathlib.Path(tmp) / 'sample.elf'
            elf.write_bytes(b'x' * 64)
            stamped = stamp_app_manifest({'file_name': elf.name, 'version': '1.0.0'}, elf)
            self.assertEqual(stamped['size_bytes'], 64)
            elf.write_bytes(b'y' * 64)
            with self.assertRaises(ValueError):
                stamp_app_manifest(stamped, elf)

    def test_bad_manifest_name_rejected(self):
        with self.assertRaises(ValueError):
            validate_manifest(ROOT / 'Apps/settings.c', pathlib.Path('wrong.elf'))


if __name__ == '__main__':
    unittest.main()
