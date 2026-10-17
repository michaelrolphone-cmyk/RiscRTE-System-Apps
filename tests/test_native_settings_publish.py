import hashlib
import copy
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
                  'bundle': publisher.TAG, 'version': publisher.VERSION, 'size_bytes': len(elf),
                  'sha256': hashlib.sha256(elf).hexdigest(),
                  'required_capabilities': capabilities, 'software_target_validation': True}
        (app/'settings.elf').write_bytes(elf)
        (app/'settings-build-record.json').write_text(json.dumps(record))
        (app/'settings.json').write_text(json.dumps({'version': publisher.VERSION, 'requires': capabilities}))
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
        self.assertEqual(publisher.VERSION, '1.4.1')
        self.assertEqual(publisher.TAG, 'native-settings-1.4.1-dev1')
        self.assertTrue(all(c[2] == publisher.TAG for c in mutations))
        self.assertFalse(any('native-settings-1.4.0-dev1' in str(c) for c in calls))

    def test_previous_version_or_mixed_identity_never_mutates_release(self):
        cases = [('settings.json', 'version', '1.4.0'),
                 ('settings-build-record.json', 'version', '1.4.0'),
                 ('settings-build-record.json', 'bundle', 'native-settings-1.4.0-dev1')]
        for filename, key, value in cases:
            with self.subTest(filename=filename, key=key), tempfile.TemporaryDirectory() as temporary:
                calls = []
                def fake(*args, **kwargs):
                    calls.append(args)
                    return subprocess.CompletedProcess([], 1, '', 'HTTP 404: Not Found')
                root = Path(temporary)
                self.fixture(root)
                path = root/'app'/filename
                data = json.loads(path.read_text())
                data[key] = value
                path.write_text(json.dumps(data))
                with self.assertRaisesRegex(RuntimeError, 'source/version identity'):
                    self.invoke(root, fake)
                self.assertFalse(any(c[:2] in [('release','create'),('release','upload'),('release','edit')]
                                     for c in calls))

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


class PreservedDev1Annotation(unittest.TestCase):
    def fixture(self):
        return {'id': publisher.DEV1_RELEASE_ID, 'tag_name': publisher.DEV1_TAG,
                'target_commitish': publisher.DEV1_SOURCE, 'name': publisher.DEV1_TAG,
                'draft': False, 'prerelease': True, 'created_at': 'created',
                'published_at': 'published', 'body': 'Original body.\n\nKeep this text verbatim.\n',
                'assets': [{'id': i, 'name': name, 'size': size, 'digest': 'sha256:'+digest}
                           for i, (name, (size, digest)) in enumerate(publisher.DEV1_ASSETS.items())]}

    def tool(self, release, source=publisher.DEV1_SOURCE, after_patch=None):
        calls = []
        base = 'repos/'+publisher.DEV1_REPOSITORY
        endpoint = base+'/releases/'+str(publisher.DEV1_RELEASE_ID)
        def fake(*args, **kwargs):
            calls.append((args, kwargs))
            if args == ('api', endpoint):
                return subprocess.CompletedProcess([], 0, json.dumps(release), '')
            if args == ('api', base+'/commits/'+publisher.DEV1_TAG, '--jq', '.sha'):
                return subprocess.CompletedProcess([], 0, source+'\n', '')
            self.assertEqual(args, ('api', endpoint, '--method', 'PATCH', '--input', '-'))
            payload = json.loads(kwargs['input'])
            self.assertEqual(set(payload), {'body'})
            release['body'] = payload['body']
            if after_patch:
                after_patch(release)
            return subprocess.CompletedProcess([], 0, json.dumps(release), '')
        return fake, calls

    def test_body_only_patch_preserves_original_and_reads_back_identity_assets(self):
        release = self.fixture()
        before = copy.deepcopy(release)
        fake, calls = self.tool(release)
        with patch.object(publisher, 'gh', fake):
            publisher.annotate_dev1(publisher.DEV1_REPOSITORY)
        self.assertEqual(release['body'], publisher.DEV1_WARNING+before['body'])
        self.assertEqual({k:v for k,v in release.items() if k != 'body'},
                         {k:v for k,v in before.items() if k != 'body'})
        self.assertEqual(len(calls), 5)
        self.assertEqual(sum('PATCH' in args for args, kwargs in calls), 1)
        for text in ('Known-limited / not recommended', '600 ms', 'date editor',
                     'No persistence was observed', publisher.DEV1_SOURCE,
                     '?comment=6e12a663-3dca-4f2b-8c8d-9af1c12fb751'):
            self.assertIn(text, release['body'])

    def test_exact_existing_warning_is_idempotent_but_still_checks_assets_and_tag(self):
        release = self.fixture()
        release['body'] = publisher.DEV1_WARNING+release['body']
        before = copy.deepcopy(release)
        fake, calls = self.tool(release)
        with patch.object(publisher, 'gh', fake):
            publisher.annotate_dev1(publisher.DEV1_REPOSITORY)
        self.assertEqual(release, before)
        self.assertEqual(len(calls), 2)
        self.assertFalse(any('PATCH' in args for args, kwargs in calls))

    def test_unrelated_public_download_does_not_invalidate_body_only_readback(self):
        release = self.fixture()
        fake, calls = self.tool(release, after_patch=lambda r: r['assets'][0].update(download_count=1))
        with patch.object(publisher, 'gh', fake):
            publisher.annotate_dev1(publisher.DEV1_REPOSITORY)
        self.assertEqual(sum('PATCH' in args for args, kwargs in calls), 1)

    def test_failed_patch_is_not_retried(self):
        fake, calls = self.tool(self.fixture())
        def fail_patch(*args, **kwargs):
            if 'PATCH' in args:
                calls.append((args, kwargs))
                raise subprocess.CalledProcessError(1, 'gh', stderr='HTTP 403')
            return fake(*args, **kwargs)
        with patch.object(publisher, 'gh', fail_patch), self.assertRaises(subprocess.CalledProcessError):
            publisher.annotate_dev1(publisher.DEV1_REPOSITORY)
        self.assertEqual(len(calls), 3)
        self.assertEqual(sum('PATCH' in args for args, kwargs in calls), 1)

    def test_identity_asset_or_marker_mismatch_never_patches(self):
        cases = {
            'release-id': lambda r: r.update(id=0),
            'tag': lambda r: r.update(tag_name=publisher.TAG),
            'draft': lambda r: r.update(draft=True),
            'stable-release': lambda r: r.update(prerelease=False),
            'missing-asset': lambda r: r['assets'].pop(),
            'extra-asset': lambda r: r['assets'].append({'name': 'extra'}),
            'duplicate-asset': lambda r: r['assets'].__setitem__(0, r['assets'][1]),
            'changed-digest': lambda r: r['assets'][0].update(digest='sha256:changed'),
            'missing-digest': lambda r: r['assets'][0].pop('digest'),
            'changed-size': lambda r: r['assets'][0].update(size=0),
            'partial-warning': lambda r: r.update(body=publisher.DEV1_MARKER+r['body']),
            'duplicate-warning': lambda r: r.update(body=publisher.DEV1_WARNING*2+r['body']),
        }
        for name, change in cases.items():
            with self.subTest(name=name):
                release = self.fixture()
                change(release)
                fake, calls = self.tool(release)
                with patch.object(publisher, 'gh', fake), self.assertRaises(RuntimeError):
                    publisher.annotate_dev1(publisher.DEV1_REPOSITORY)
                self.assertFalse(any('PATCH' in args for args, kwargs in calls))

    def test_wrong_repository_or_source_never_patches(self):
        with patch.object(publisher, 'gh') as api, self.assertRaises(RuntimeError):
            publisher.annotate_dev1('owner/another-repo')
        api.assert_not_called()
        fake, calls = self.tool(self.fixture(), source='changed')
        with patch.object(publisher, 'gh', fake), self.assertRaisesRegex(RuntimeError, 'source tag'):
            publisher.annotate_dev1(publisher.DEV1_REPOSITORY)
        self.assertFalse(any('PATCH' in args for args, kwargs in calls))

    def test_readback_drift_is_reported_without_retry_or_other_mutation(self):
        changes = [lambda r: r.update(body='unexpected replacement'),
                   lambda r: r.update(name='changed name'),
                   lambda r: r['assets'][0].update(id=999)]
        for change in changes:
            fake, calls = self.tool(self.fixture(), after_patch=change)
            with patch.object(publisher, 'gh', fake), self.assertRaisesRegex(RuntimeError, 'readback'):
                publisher.annotate_dev1(publisher.DEV1_REPOSITORY)
            self.assertEqual(sum('PATCH' in args for args, kwargs in calls), 1)

    def test_annotation_cli_cannot_publish_successor(self):
        with patch.object(sys, 'argv', ['publish', '--repository', publisher.DEV1_REPOSITORY,
                                       '--annotate-dev1']), \
                patch.object(publisher, 'annotate_dev1') as annotate, \
                patch.object(publisher, 'gh') as api:
            publisher.main()
        annotate.assert_called_once_with(publisher.DEV1_REPOSITORY)
        api.assert_not_called()
