import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'watch19_custody', ROOT / 'scripts/verify_watch19_system_custody.py')
custody = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(custody)


class Watch19CustodyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        for name in ('manifest.json', 'System.bundle'):
            shutil.copyfile(custody.CUSTODY / name, self.directory / name)

    def test_preserved_bytes_and_mapping(self):
        manifest, _ = custody.read_custody(self.directory)
        self.assertEqual(manifest['reconstruction_commit'], custody.SOURCE)

    def test_modified_bundle_is_rejected(self):
        path = self.directory / 'System.bundle'
        payload = bytearray(path.read_bytes())
        payload[-1] ^= 1
        path.write_bytes(payload)
        with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
            custody.read_custody(self.directory)

    def test_changed_prerequisite_is_rejected(self):
        path = self.directory / 'manifest.json'
        manifest = json.loads(path.read_text())
        manifest['prerequisite_commit'] = custody.PUBLIC
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'fixed source mapping'):
            custody.read_custody(self.directory)

    def test_truncated_bundle_is_rejected(self):
        path = self.directory / 'System.bundle'
        path.write_bytes(path.read_bytes()[:-1])
        with self.assertRaisesRegex(ValueError, 'byte count mismatch'):
            custody.read_custody(self.directory)
