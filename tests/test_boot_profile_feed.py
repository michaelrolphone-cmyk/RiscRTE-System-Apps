#!/usr/bin/env python3
"""Offline byte-custody fixtures only: no executable product or candidate builds."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import build_boot_profile_feed as feed

RUNTIME = Path(os.environ['RUNTIME_REPO']) if os.environ.get('RUNTIME_REPO') else None
BASE = 'https://example.test/immutable-fixtures/assets/'
PRODUCT = 'test-watch'


def encode(value):
    return (json.dumps(value, separators=(',', ':'), sort_keys=True) + '\n').encode()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def image_fixture(files, size=0x550000):
    """Small-file SPIFFS byte fixture, not a product packer; never executes ELFs."""
    raw = bytearray(b'\xff' * size)
    blocks = size // 4096
    for block in range(blocks):
        struct.pack_into('<H', raw, block * 4096 + 252,
                         (0x20140529 ^ 256 ^ (blocks - block)) & 0xffff)
    next_page = 1

    def allocate(obj):
        nonlocal next_page
        if next_page % 16 == 0:
            next_page += 1
        page = next_page
        next_page += 1
        struct.pack_into('<H', raw, page // 16 * 4096 + (page % 16 - 1) * 2, obj)
        return page

    for obj, (name, data) in enumerate(sorted(files.items()), 1):
        assert 0 < len(data) <= 251 * 103 and len(name.encode()) <= 30
        header = allocate(obj | 0x8000) * 256
        struct.pack_into('<HHB', raw, header, obj | 0x8000, 0, 0xf8)
        struct.pack_into('<IB', raw, header + 8, len(data), 1)
        name = ('/' + name).encode() + b'\0'
        raw[header + 13:header + 13 + len(name)] = name
        for span, offset in enumerate(range(0, len(data), 251)):
            page = allocate(obj)
            struct.pack_into('<HHB', raw, page * 256, obj, span, 0xfc)
            chunk = data[offset:offset + 251]
            raw[page * 256 + 5:page * 256 + 5 + len(chunk)] = chunk
            struct.pack_into('<H', raw, header + 49 + span * 2, page)
    return bytes(raw)


def fixture_files(mirrors=True, count=10, layout='riscrte-paired-appdata-v3'):
    abi = feed.LAYOUT_ABI[layout]
    firmware = (b'NONEXECUTABLE-TEST-FIXTURE\0RISC_RUNTIME_VERSION:0.2.24\0'
                + b'RISC_PAIRED_STORE_ABI:' + str(abi).encode() + b'\0')
    app = b'NONEXECUTABLE-APPLICATION-FIXTURE' * 12
    driver = b'NONEXECUTABLE-DRIVER-FIXTURE' * 15
    boot = {'board': 'board.json', 'default_app': 'default.elf',
            'drivers': [{'manifest': 'drv/manifest.json', 'instance_id': 7}],
            'app_capabilities': [{'manifest': 'default.json', 'grants': []}],
            'provider_activation': 'demand-retained'}
    if mirrors:
        for row, directory, identity, data in (
                (boot['drivers'][0], 'Drivers', 'fixture-driver', driver),
                (boot['app_capabilities'][0], 'Apps', 'fixture-app', app)):
            row['image_source'] = {'volume_instance': 12, 'internal_fallback': True,
                                   'path': f'{directory}/{identity}/image-{sha(data)}.elf'}
    cohort = {'schema': 'riscrte.cohort', 'schema_version': 1, 'product': PRODUCT,
              'version': '1.2.3', 'runtime_version': '0.2.24', 'layout': layout,
              'source_repo': 'fixtures/test-only', 'source_revision': '1' * 40,
              'store_abi': abi, 'firmware_size': len(firmware), 'firmware_sha256': sha(firmware)}
    files = {'boot.json': encode(boot), 'board.json': encode({'fixture': 'not a hardware board'}),
             'cohort.json': encode(cohort), 'default.elf': app,
             'default.json': encode({'type': 'application', 'id': 'fixture-app', 'file_name': 'default.elf'}),
             'drv/manifest.json': encode({'type': 'driver', 'id': 'fixture-driver', 'file_name': 'main.elf'}),
             'drv/main.elf': driver,
             'assets/icon.bin': b'PRESERVE-UNREFERENCED-VALID-ICON',
             'assets/settings.json': encode({'fixture': 'valid extra JSON asset'})}
    for i in range(len(files), count):
        files[f'asset-{i:03}.bin'] = b'valid asset fixture'
    return files, firmware


def write_fixture(root, files, firmware):
    store = root / 'store'
    store.mkdir(exist_ok=True)
    for path in sorted(store.rglob('*'), reverse=True):
        if path.is_file() or path.is_symlink():
            path.unlink()
        elif path.is_dir():
            path.rmdir()
    for name, data in files.items():
        path = store / name
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_bytes(data)
    layout = json.loads(files['cohort.json'])['layout']
    partition = {'riscrte-paired-16m-v1': 0x4f0000,
                 'riscrte-paired-appdata-v2': 0x510000,
                 'riscrte-paired-appdata-v3': 0x550000}.get(layout, 0x550000)
    (root / 'bootfs.fixture').write_bytes(image_fixture(files, partition))
    (root / 'native.fixture').write_bytes(firmware)


@unittest.skipUnless(RUNTIME, 'Set RUNTIME_REPO to a schema-4/160-file Runtime checkout')
class BootProfileFeed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.helpers = feed.load_runtime(RUNTIME)
        cls.p = cls.helpers.profile

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.files, self.firmware = fixture_files()
        self.reset()

    def reset(self):
        write_fixture(self.root, self.files, self.firmware)

    def build(self, output='out', **kwargs):
        args = dict(runtime=RUNTIME, store=self.root / 'store', image=self.root / 'bootfs.fixture',
                    firmware=self.root / 'native.fixture', base_url=BASE, product=PRODUCT,
                    profile='core', servers=['time.example.test'], output=self.root / output)
        args.update(kwargs)
        return feed.build(**args)

    def read_feed(self, output='out'):
        return json.loads((self.root / output / 'native-packages-v1.json').read_bytes())

    def reject(self, pattern, **kwargs):
        with self.assertRaisesRegex((ValueError, AssertionError), pattern):
            self.build(**kwargs)
        self.assertFalse((self.root / 'out').exists())

    def edit_json(self, name, mutate):
        data = json.loads(self.files[name])
        mutate(data)
        self.files[name] = encode(data)
        self.reset()

    def test_exact_envelope_inventory_asset_custody_and_determinism(self):
        receipt = self.build()
        self.build(output='again')
        doc = self.read_feed()
        self.assertEqual(set(doc), {'schema', 'version', 'product', 'apps', 'profiles', 'boot_profiles'})
        self.assertEqual((doc['schema'], doc['version'], doc['product']),
                         ('riscrte.native-packages', 1, PRODUCT))
        self.assertEqual(doc['apps'], [])
        self.assertEqual(doc['profiles'], {'core': []})
        core = doc['boot_profiles']['core']
        self.assertEqual(set(core), {'native', 'profile', 'time', 'mirrors'})
        cohort = json.loads(self.files['cohort.json'])
        self.assertEqual(core['native'], {key: cohort[key] for key in feed.NATIVE_KEYS})
        public = core['profile']
        self.assertEqual(set(public), {'schema', 'schema_version', 'image', 'files'})
        self.assertEqual(public['schema_version'], 4)
        self.assertEqual(public['files'], [[path, len(data), sha(data)] for path, data in sorted(self.files.items())])
        raw = (self.root / 'bootfs.fixture').read_bytes()
        self.assertEqual(public['image'], {'url': BASE + f'bootfs-{sha(raw)}.bin', 'bytes': len(raw), 'sha256': sha(raw)})
        self.assertEqual(core['time'], {'schema': 'riscrte.sntp', 'schema_version': 1, 'servers': ['time.example.test']})
        self.assertEqual(len(core['mirrors']), 2)
        for mirror in core['mirrors']:
            self.assertEqual(set(mirror), {'driver', 'path', 'url', 'size', 'sha256'})
            expected = self.files['drv/main.elf' if mirror['driver'] else 'default.elf']
            self.assertEqual((mirror['size'], mirror['sha256']), (len(expected), sha(expected)))
            self.assertEqual((self.root / 'out/assets' / mirror['url'].rsplit('/', 1)[1]).read_bytes(), expected)
        self.assertEqual(receipt['file_count'], len(self.files))
        self.assertTrue(receipt['credential_free'])
        self.assertFalse(receipt['published'])
        self.assertEqual(self.helpers.read_image(raw, len(raw)), self.files)
        for path in (self.root / 'out').rglob('*'):
            if path.is_file():
                self.assertEqual(path.read_bytes(), (self.root / 'again' / path.relative_to(self.root / 'out')).read_bytes())
        for line in (self.root / 'out/SHA256SUMS').read_text().splitlines():
            digest, path = line.split('  ', 1)
            self.assertEqual(digest, sha((self.root / 'out' / path).read_bytes()))
        self.assertEqual((self.root / 'out/COMPLETE').read_bytes(), b'riscrte.boot-profile-feed.v1\n')
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.build()

    def test_full_160_file_inventory_and_native_consumers(self):
        self.files, self.firmware = fixture_files(count=160)
        self.reset()
        receipt = self.build()
        self.assertEqual(receipt['file_count'], 160)
        self.assertLessEqual(receipt['profile_bytes'], 15872)
        for key in ('BOOT_PROFILE_NATIVE_TEST', 'BOOT_PROFILE_CONSUMER_TEST'):
            if os.environ.get(key):
                subprocess.run([os.environ[key], str(self.root / 'out/native-packages-v1.json')], check=True)

    def test_internal_only_all_supported_layouts_and_boot_policy_preserved(self):
        for layout in feed.LAYOUT_ABI:
            with self.subTest(layout=layout):
                self.files, self.firmware = fixture_files(mirrors=False, layout=layout)
                boot = json.loads(self.files['boot.json'])
                # Byte custody preserves product-owned admission policy; this
                # fixture is not native graph/board/ELF qualification.
                for key in ('port', 'cohort_migration', 'resident_shell', 'installation'):
                    boot[key] = {'fixture': key}
                self.files['boot.json'] = encode(boot)
                self.reset()
                self.build(output=layout)
                self.assertEqual(self.read_feed(layout)['boot_profiles']['core']['mirrors'], [])
                self.assertEqual(self.helpers.read_image((self.root / 'bootfs.fixture').read_bytes(),
                                                        self.p.LAYOUTS[layout]), self.files)

    def test_complete_store_mismatch_even_for_nonmanifest_assets(self):
        original = dict(self.files)
        for action in ('changed', 'extra', 'missing'):
            with self.subTest(action=action):
                self.files = dict(original)
                self.reset()
                asset = self.root / 'store/assets/icon.bin'
                if action == 'changed':
                    asset.write_bytes(b'altered unreferenced valid asset')
                elif action == 'extra':
                    (self.root / 'store/extra.bin').write_bytes(b'not in image')
                else:
                    asset.unlink()
                self.reject('image and complete product store differ')

    def test_image_corruption_length_physical_pages_and_decoded_bytes(self):
        raw = (self.root / 'bootfs.fixture').read_bytes()
        for corrupt in ('length', 'magic', 'free-page', 'live-data'):
            with self.subTest(corrupt=corrupt):
                modified = bytearray(raw)
                if corrupt == 'length':
                    modified.pop()
                elif corrupt == 'magic':
                    modified[252] ^= 1
                elif corrupt == 'free-page':
                    modified[-1] = 0
                else:
                    modified[2 * 256 + 5] ^= 1
                (self.root / 'bootfs.fixture').write_bytes(modified)
                self.reject('')

    def test_cohort_binding_schema_product_layout_firmware(self):
        original = dict(self.files)
        variants = [('schema', 'other'), ('schema_version', True), ('product', 'other'),
                    ('layout', 'unknown'), ('store_abi', True), ('store_abi', 2),
                    ('firmware_size', True), ('firmware_size', len(self.firmware) + 1),
                    ('firmware_sha256', 'a' * 64), ('runtime_version', '0.2.25'),
                    ('runtime_version', '00.2.24'), ('version', '4294967296.1.1'),
                    ('source_revision', '1' * 39), ('source_repo', '../private')]
        for key, value in variants:
            with self.subTest(key=key, value=value):
                self.files = dict(original)
                self.edit_json('cohort.json', lambda data: data.update({key: value}))
                self.reject('')
        self.files = original
        self.reset()
        (self.root / 'native.fixture').write_bytes(self.firmware + b'changed')
        self.reject('firmware binding')

    def test_native_embedded_marker_missing_conflicting_or_wrong_abi(self):
        original = self.firmware
        for change in (original.replace(b'RISC_RUNTIME_VERSION:', b'OTHER_VERSION_MARKER:'),
                       original + b'RISC_RUNTIME_VERSION:0.2.25\0',
                       original.replace(b'ABI:3', b'ABI:2')):
            self.firmware = change
            self.edit_json('cohort.json', lambda data: data.update(firmware_size=len(change), firmware_sha256=sha(change)))
            self.reject('compiled version or ABI mismatch')

    def test_credentials_duplicate_json_and_missing_manifest_rejected(self):
        original = dict(self.files)
        for key in ('wifi', 'SSID', 'password', 'passphrase', 'PSK', 'credentials', 'private-key', 'api_key', 'access_token', 'refresh_token'):
            with self.subTest(key=key):
                self.files = dict(original)
                self.edit_json('assets/settings.json', lambda data: data.update(nested=[{key: 'never publish this'}]))
                self.reject('credentials forbidden')
        self.files = dict(original)
        self.files['default.json'] = b'{"type":"application","type":"application"}'
        self.reset()
        self.reject('duplicate JSON field')
        self.files = dict(original)
        del self.files['default.json']
        self.reset()
        self.reject('admitted manifest missing')

    def test_mirror_requires_authorized_source_and_exact_internal_bytes(self):
        original = dict(self.files)
        for key, value in [('volume_instance', 11), ('volume_instance', True),
                           ('internal_fallback', 1),
                           ('path', 'Apps/fixture-app/image-' + '0' * 64 + '.elf'),
                           ('path', '/Apps/fixture-app/image-' + '0' * 64 + '.elf'),
                           ('path', 'Apps/../image.elf'), ('unexpected', 1)]:
            with self.subTest(key=key, value=value):
                self.files = dict(original)
                self.edit_json('boot.json', lambda data: data['app_capabilities'][0]['image_source'].update({key: value}))
                self.reject('')
        self.files = dict(original)
        del self.files['drv/main.elf']
        self.reset()
        self.reject('internal fallback image missing')
        self.files = dict(original)
        self.edit_json('default.json', lambda data: data.update(size_bytes=1, sha256='0' * 64))
        self.reject('manifest internal image binding mismatch')

    def test_preserve_legacy_external_only_source_without_internal_image(self):
        for fallback in (None, False):
            with self.subTest(fallback=fallback):
                self.files, self.firmware = fixture_files()
                self.files['legacy.json'] = encode({'type': 'application', 'id': 'legacy-app',
                                                    'file_name': 'legacy-' + 'x' * 60 + '.elf'})
                boot = json.loads(self.files['boot.json'])
                source = {'volume_instance': 12, 'path': 'Apps/legacy-app/legacy.elf'}
                if fallback is not None:
                    source['internal_fallback'] = fallback
                boot['app_capabilities'].append({'manifest': 'legacy.json', 'grants': [], 'image_source': source})
                self.files['boot.json'] = encode(boot)
                self.reset()
                name = 'legacy-' + str(fallback)
                self.build(output=name)
                core = self.read_feed(name)['boot_profiles']['core']
                self.assertEqual(len(core['mirrors']), 2)
                self.assertEqual({row[0] for row in core['profile']['files']}, set(self.files))
                raw = (self.root / 'bootfs.fixture').read_bytes()
                self.assertEqual(self.helpers.read_image(raw, len(raw))['boot.json'], self.files['boot.json'])

    def test_mirror_id_size_count_and_deduplication_bounds(self):
        def records(count, identity=None):
            data = b'nonexecutable fixture'
            result = []
            for i in range(count):
                name = identity or f'fixture-{i}'
                row = {'image_source': {'volume_instance': 12, 'internal_fallback': True,
                                       'path': f'Apps/{name}/image-{sha(data)}.elf'}}
                result.append((False, row, {'id': name}, 'image.elf'))
            return result, {'image.elf': data}
        rows, files = records(64)
        mirrors, assets = feed.mirror_assets(self.p, rows, files, BASE)
        self.assertEqual((len(mirrors), len(assets)), (64, 1))
        rows, files = records(65)
        with self.assertRaisesRegex(ValueError, 'too many'):
            feed.mirror_assets(self.p, rows, files, BASE)
        for identity in ('BadID', 'dot.id'):
            rows, files = records(1, identity)
            with self.assertRaisesRegex(ValueError, 'package ID'):
                feed.mirror_assets(self.p, rows, files, BASE)
        rows, files = records(1)
        files['image.elf'] = b'x' * (feed.MAX_MIRROR_BYTES + 1)
        with self.assertRaisesRegex(ValueError, 'consumer bound'):
            feed.mirror_assets(self.p, rows, files, BASE)
        rows, files = records(2, 'same-id')
        self.assertEqual(len(feed.mirror_assets(self.p, rows, files, BASE)[0]), 1)

    def test_161_files_and_public_profile_headroom(self):
        self.files, self.firmware = fixture_files(count=161)
        self.reset()
        self.reject('too many product store files')
        self.files, self.firmware = fixture_files(count=160)
        # Paths remain valid native SPIFFS names, but the exact tuple inventory
        # must still leave at least 512 bytes for service-owned Wi-Fi injection.
        self.files = {('a' * (30 - len(name)) + name if name.startswith('asset-') else name): data
                      for name, data in self.files.items()}
        self.reset()
        self.reject('Wi-Fi headroom')

    def test_exact_public_profile_boundary_and_escaped_wifi_headroom(self):
        self.files, self.firmware = fixture_files(count=160)
        # Content-addressed names have fixed length, so compute the exact
        # profile length before constructing this test-only physical image.
        def profile_size():
            profile = {'schema': 'riscrte.provisioning', 'schema_version': 4,
                       'image': {'url': BASE + 'bootfs-' + '0' * 64 + '.bin',
                                 'bytes': 0x550000, 'sha256': '0' * 64},
                       'files': [[name, len(data), sha(data)] for name, data in sorted(self.files.items())]}
            return len(self.p.encode(profile))
        remaining = 15872 - profile_size()
        self.assertGreater(remaining, 0)
        for name in list(self.files):
            if name.startswith('asset-'):
                amount = min(remaining, 30 - len(name))
                self.files['a' * amount + name] = self.files.pop(name)
                remaining -= amount
                if not remaining:
                    break
        self.assertEqual(remaining, 0)
        self.assertEqual(profile_size(), 15872)
        self.reset()
        self.assertEqual(self.build(output='boundary')['profile_bytes'], 15872)
        for key in ('BOOT_PROFILE_NATIVE_TEST', 'BOOT_PROFILE_CONSUMER_TEST'):
            if os.environ.get(key):
                subprocess.run([os.environ[key], str(self.root / 'boundary/native-packages-v1.json')], check=True)
        # One more safe filename byte must fail before output exists.
        name = next(name for name in self.files if name.startswith('asset-') and len(name) < 30)
        self.files['a' + name] = self.files.pop(name)
        self.reset()
        self.reject('Wi-Fi headroom')

    def test_explicit_time_and_canonical_https_are_required(self):
        for servers in (None, [], ['one'] * 2, ['one', 'two', 'three', 'four'],
                        ['https://time.test/'], ['bad name'], ['.start'], ['end.'], ['x' * 64]):
            with self.subTest(servers=servers):
                self.reject('', servers=servers)
        for url in ('http://example.test/', 'https://user:pass@example.test/',
                    'https://example.test:443/', 'https://example.test/a?token=x/',
                    'https://example.test/a#fragment/', 'https://example.test/../',
                    'https://example.test/%2e/', 'https://example.test//',
                    'https://example.test/no-slash', 'https://bad..test/'):
            with self.subTest(url=url):
                self.reject('', base_url=url)
        for bad in ({'product': ''}, {'product': '../escape'}, {'profile': 'extras'}):
            self.reject('', **bad)
        self.build(servers=['a.example.test', 'b.example.test', '192.0.2.10'])
        self.assertEqual(len(self.read_feed()['boot_profiles']['core']['time']['servers']), 3)

    def test_unsafe_paths_inputs_and_output_separation(self):
        image = self.root / 'bootfs.fixture'
        image.unlink()
        image.symlink_to(self.root / 'native.fixture')
        self.reject('symlink')
        image.unlink()
        self.reset()
        (self.root / 'store/linked').symlink_to(self.root / 'native.fixture')
        self.reject('symlink')
        (self.root / 'store/linked').unlink()
        self.reject('separate', output='store/out')
        self.reject('already exists', output='store')
        (self.root / 'alias').mkdir()
        self.reject('separate', output='alias/../store/out')

    def test_output_interruption_removes_partial_bundle_and_retry_succeeds(self):
        # load_runtime deliberately produces fresh isolated helpers each call.
        helpers = feed.load_runtime(RUNTIME)
        original = helpers.profile.write
        def interrupted(path, data):
            if path.name == 'native-packages-v1.json':
                raise KeyboardInterrupt()
            original(path, data)
        with patch.object(feed, 'load_runtime', return_value=helpers), patch.object(helpers.profile, 'write', side_effect=interrupted):
            with self.assertRaises(KeyboardInterrupt):
                self.build()
        self.assertFalse((self.root / 'out').exists())
        self.build()

    def test_changed_snapshot_fails_before_output(self):
        helpers = feed.load_runtime(RUNTIME)
        original = helpers.profile.store_files
        calls = 0
        def changed(path):
            nonlocal calls
            calls += 1
            result = original(path)
            if calls == 2:
                result['assets/icon.bin'] += b'changed'
            return result
        with patch.object(feed, 'load_runtime', return_value=helpers), patch.object(helpers.profile, 'store_files', side_effect=changed):
            self.reject('inputs changed')

    def test_runtime_module_isolation_and_old_bounds_rejected(self):
        sentinel = object()
        with patch.dict(sys.modules, {'provision_profile': sentinel}):
            self.assertIsNot(feed.load_runtime(RUNTIME).profile, sentinel)
            self.assertIs(sys.modules['provision_profile'], sentinel)
        runtime = self.root / 'old-runtime'
        (runtime / 'scripts').mkdir(parents=True)
        for name in ('check_versions', 'release_assets', 'provision_profile', 'spiffs_image', 'store_image_capacity'):
            (runtime / 'scripts' / (name + '.py')).write_text('MAX_FILES=128\nMAX_PROFILE=16384\n')
        with self.assertRaisesRegex(ValueError, 'schema 4 and 160'):
            feed.load_runtime(runtime)
        linked = self.root / 'linked-runtime'
        linked.symlink_to(RUNTIME, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlinks'):
            feed.load_runtime(linked)

    def test_cli_requires_time_and_does_not_echo_secret_inputs(self):
        command = [sys.executable, str(ROOT / 'scripts/build_boot_profile_feed.py'),
                   '--runtime', str(RUNTIME), '--store', str(self.root / 'store'),
                   '--image', str(self.root / 'bootfs.fixture'), '--firmware', str(self.root / 'native.fixture'),
                   '--output', str(self.root / 'out'), '--base-url', BASE,
                   '--product', PRODUCT, '--profile', 'core']
        missing = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(missing.returncode, 0)
        self.assertFalse((self.root / 'out').exists())
        self.edit_json('assets/settings.json', lambda data: data.update(password='secret-marker-never-echo'))
        rejected = subprocess.run(command + ['--time-server', 'time.example.test'], capture_output=True, text=True)
        self.assertEqual(rejected.returncode, 1)
        self.assertNotIn('secret-marker', rejected.stdout + rejected.stderr)
        self.assertFalse((self.root / 'out').exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
