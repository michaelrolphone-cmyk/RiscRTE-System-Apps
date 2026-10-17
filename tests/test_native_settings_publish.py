import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native_publish', ROOT/'scripts/publish_native_settings.py')
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


class NativeSettingsPublication(unittest.TestCase):
    def fixture(self, root):
        app = root/'app'
        app.mkdir()
        elf = b'qualified fixture ELF'
        capabilities = [{'capability': str(i), 'api': 1} for i in range(16)]
        record = {'repository_commit': 'new', 'working_tree_dirty': False,
                  'bundle': publisher.TAG, 'size_bytes': len(elf),
                  'sha256': hashlib.sha256(elf).hexdigest(),
                  'required_capabilities': capabilities, 'software_target_validation': True}
        (app/'settings.elf').write_bytes(elf)
        (app/'settings-build-record.json').write_text(json.dumps(record))
        (app/'settings.json').write_text(json.dumps({'version': '1.4.0', 'requires': capabilities}))
        for name in ('command.json', 'memory-admission.json'):
            (app/name).write_text('{}')

    def invoke(self, root, fake):
        with patch.object(sys, 'argv', ['publish', '--artifact', str(root),
                                       '--repository', 'owner/repo', '--source-sha', 'new']), \
             patch.object(publisher, 'gh', fake):
            publisher.main()

    def test_new_release_uses_qualified_source_and_draft_before_upload(self):
        calls = []
        def fake(*args, **kwargs):
            calls.append(args)
            if args[:2] == ('release', 'view') or args[0] == 'api':
                return subprocess.CompletedProcess([], 1, '', 'HTTP 404: Not Found')
            if args[:2] == ('release', 'upload'):
                paths = [Path(x) for x in args[5:]]
                self.assertEqual({x.name for x in paths}, publisher.REQUIRED)
                self.assertTrue(all(x.is_file() for x in paths))
                sums = (paths[0].parent/'SHA256SUMS').read_text()
                self.assertIn(hashlib.sha256((paths[0].parent/'settings.elf').read_bytes()).hexdigest(), sums)
            return subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root)
            self.invoke(root, fake)
        mutations = [c for c in calls if c[:2] in [('release','create'),('release','upload'),('release','edit')]]
        self.assertEqual([c[1] for c in mutations], ['create','upload','edit'])
        self.assertIn('--draft', mutations[0])
        self.assertEqual(mutations[0][mutations[0].index('--target')+1], 'new')
        self.assertIn('--draft=false', mutations[-1])

    def test_existing_release_verifies_original_source_and_skips_new_source(self):
        calls = []
        def fake(*args, **kwargs):
            calls.append(args)
            if args[:2] == ('release','view'):
                return subprocess.CompletedProcess([], 0, json.dumps({'isDraft':False,'isPrerelease':True,
                    'assets':[{'name':n} for n in publisher.REQUIRED]}), '')
            if args[:2] == ('release','download'):
                (Path(args[args.index('--dir')+1])/'publication.json').write_text(
                    json.dumps({'tag':publisher.TAG,'source_commit':'old'}))
            return subprocess.CompletedProcess([], 0, 'old\n', '')
        self.invoke(Path('/does-not-exist'), fake)
        self.assertFalse(any(c[:2] in [('release','create'),('release','upload'),('release','edit')] for c in calls))

    def test_wrong_source_or_prior_tag_never_mutates_release(self):
        for existing_tag in (False, True):
            calls = []
            def fake(*args, **kwargs):
                calls.append(args)
                if args[0]=='api' and existing_tag:
                    return subprocess.CompletedProcess([],0,'old\n','')
                return subprocess.CompletedProcess([],1,'','HTTP 404: Not Found')
            with tempfile.TemporaryDirectory() as temporary:
                root=Path(temporary)
                self.fixture(root)
                record=root/'app/settings-build-record.json'
                data=json.loads(record.read_text());data['repository_commit']='wrong';record.write_text(json.dumps(data))
                with self.assertRaises(RuntimeError):
                    self.invoke(root,fake)
            self.assertFalse(any(c[:2] in [('release','create'),('release','upload'),('release','edit')] for c in calls))
