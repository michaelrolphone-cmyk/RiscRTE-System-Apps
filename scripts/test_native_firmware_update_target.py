#!/usr/bin/env python3
"""Offline negative controls for the complete native Firmware Update target."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import build_native_app_store_qualification as app_store
import build_native_firmware_update_qualification as target
import report_native_firmware_update_memory as admission


class NativeFirmwareUpdateTargetTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.out = Path(self.tmp.name)
        self.repos = {key: self.out/key for key in target.PINS}
        self.version = json.loads((target.ROOT/target.VERSION_PROFILE).read_text())['version']

    def tearDown(self):
        self.tmp.cleanup()

    def arguments(self):
        return target.compose_arguments((target.ROOT/target.RECIPE).read_bytes(), self.repos,
            self.out, self.out/'include', self.out/'profile', self.out/'runtime-sdk', self.version)

    def test_complete_retained_firmware_recipe(self):
        args = self.arguments()
        target.validate_recipe(args, self.version)
        self.assertEqual(len(target.CAPABILITIES), 16)
        self.assertIn(('software.update.firmware', 1), target.CAPABILITIES)
        self.assertNotIn(('software.update.apps', 1), target.CAPABILITIES)
        self.assertNotIn(('platform.bank-store', 1), target.CAPABILITIES)
        self.assertNotIn(('app.catalog', 1), target.CAPABILITIES)
        self.assertIn('-DPORTABLE_UPDATE_FIRMWARE=1', args)
        self.assertIn(str(target.ROOT/target.CATALOG_PROFILE), args)
        self.assertEqual({Path(a).name for a in args if a.endswith('.c')}, target.REQUIRED_SOURCES)
        self.assertFalse(any('@' in a for a in args))
        for selector in target.DEFINES:
            self.assertEqual(args.count('-D'+selector), 1)

    def test_manifest_is_full_recipe_not_legacy_minimum(self):
        actual = json.loads((target.ROOT/target.APP_MANIFEST).read_text())
        self.assertEqual(actual, target.manifest(self.version))
        self.assertEqual(actual['id'], 'ota_update')
        self.assertEqual(actual['file_name'], 'ota_update.elf')
        self.assertEqual(set(target.CAPABILITY_SELECTORS), {r['capability'] for r in actual['requires']})
        self.assertEqual(target.CAPABILITY_SELECTORS['software.update.firmware'], 'PORTABLE_UPDATE_APP')

    def test_every_recipe_argument_required(self):
        args = self.arguments()
        for index, arg in enumerate(args):
            with self.subTest(argument=arg), self.assertRaises(ValueError):
                target.validate_recipe(args[:index]+args[index+1:], self.version)

    def test_every_argument_position_preserved(self):
        args = self.arguments()
        for index in range(len(args)-1):
            if args[index] == args[index+1]:
                continue
            changed = args.copy()
            changed[index], changed[index+1] = changed[index+1], changed[index]
            with self.subTest(index=index), self.assertRaises(ValueError):
                target.validate_recipe(changed, self.version)

    def test_extra_flags_and_sources_rejected(self):
        args = self.arguments()
        for flag in ('-DPORTABLE_UPDATE_FIRMWARE=0', '-UPORTABLE_UPDATE_NATIVE_UI',
                     '-UPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_RUNTIME_APP_CATALOG',
                     '-DPORTABLE_UPDATE_VERSION="9.0.0"', '-O0', '-UUPDATE_RETURN_APP',
                     '-lunsafe', '/tmp/service.cpp', '/tmp/extra.c', '-I/tmp/unowned'):
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                target.validate_recipe(args+[flag], self.version)

    def test_duplicate_selectors_rejected(self):
        args = self.arguments()
        for arg in [a for a in args if a.startswith('-D')]:
            with self.subTest(argument=arg), self.assertRaises(ValueError):
                target.validate_recipe(args+[arg], self.version)

    def test_native_selectors_cannot_be_zeroed(self):
        args = self.arguments()
        for selector in target.DEFINES:
            changed = [a+'=0' if a == '-D'+selector else a for a in args]
            with self.subTest(selector=selector), self.assertRaises(ValueError):
                target.validate_recipe(changed, self.version)

    def test_every_fixed_value_preserved(self):
        args = self.arguments()
        for name in target.REQUIRED_VALUES:
            changed = ['-D'+name+'=999' if a.startswith('-D'+name+'=') else a for a in args]
            with self.subTest(selector=name), self.assertRaises(ValueError):
                target.validate_recipe(changed, self.version)

    def test_materialized_include_paths_cannot_be_substituted(self):
        args = self.arguments()
        for index, arg in enumerate(args):
            if arg.startswith('-I'):
                changed = args.copy()
                changed[index] = '-I/tmp/unowned'
                with self.subTest(argument=arg), self.assertRaises(ValueError):
                    target.validate_recipe(changed, self.version)

    def test_materialized_source_or_forced_include_cannot_be_substituted(self):
        args = self.arguments()
        for index, arg in enumerate(args):
            if arg.endswith(('.c', '.h')):
                changed = args.copy()
                changed[index] = '/tmp/unowned/'+Path(arg).name
                with self.subTest(argument=arg), self.assertRaises(ValueError):
                    target.validate_recipe(changed, self.version)

    def test_linker_map_cannot_be_substituted(self):
        args = self.arguments()
        changed = ['-Wl,--version-script=/tmp/unowned/ota_update.map'
                   if a.startswith('-Wl,--version-script=') else a for a in args]
        with self.assertRaises(ValueError):
            target.validate_recipe(changed, self.version)

    def test_version_must_match_compilation(self):
        with self.assertRaises(ValueError):
            target.validate_recipe(self.arguments(), '9.0.0')

    def test_retained_recipe_bytes_cannot_be_modified(self):
        data = (target.ROOT/target.RECIPE).read_bytes()
        for changed in (b'{}', data+b'\n', data.replace(b'PORTABLE_UPDATE_FIRMWARE=1', b'PORTABLE_UPDATE_FIRMWARE=0')):
            with self.subTest(digest=hashlib.sha256(changed).hexdigest()), self.assertRaises(ValueError):
                target.compose_arguments(changed, self.repos, self.out, self.out/'include',
                    self.out/'profile', self.out/'runtime-sdk', self.version)

    def test_recipe_validation_rechecks_digest(self):
        args = self.arguments()
        recipe = self.out/target.RECIPE
        recipe.parent.mkdir(parents=True)
        recipe.write_bytes((target.ROOT/target.RECIPE).read_bytes()+b'\n')
        with mock.patch.object(target, 'ROOT', self.out), self.assertRaises(ValueError):
            target.validate_recipe(args, self.version)

    def test_service_contract_is_unchanged_from_pinned_baseline(self):
        actual = target.service_contract()
        self.assertEqual(set(actual), set(target.SERVICE_CONTRACT_INPUTS))
        for relative, digest in actual.items():
            expected = target.git(target.ROOT, 'show', target.SOURCE_BASELINE+':'+relative)
            self.assertEqual(digest, hashlib.sha256(expected).hexdigest())

    def test_changed_service_contract_refused(self):
        originals = {relative: (target.ROOT/relative).read_bytes() for relative in target.SERVICE_CONTRACT_INPUTS}
        for relative, data in originals.items():
            path = self.out/relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        def original_git(repo, verb, revision):
            self.assertEqual(repo, self.out)
            self.assertEqual(verb, 'show')
            return originals[revision.split(':', 1)[1]]
        with mock.patch.object(target, 'ROOT', self.out), mock.patch.object(target, 'git', side_effect=original_git):
            for relative, data in originals.items():
                path = self.out/relative
                path.write_bytes(data+b'\n')
                with self.subTest(path=relative), self.assertRaises(ValueError):
                    target.service_contract()
                path.write_bytes(data)

    def test_existing_profiles_and_builder_are_byte_identical(self):
        paths = ('Apps/ota_update.json', 'Apps/native/ota_update.json',
                 'Apps/app_store.json', 'Apps/native/app_store.json',
                 'lib/PortableApps/profiles/native-app-store.json',
                 'lib/PortableApps/profiles/native-app-store-manifest.json',
                 'profiles/native-app-store/empty_catalog.c',
                 'scripts/build_native_app_store_qualification.py',
                 'scripts/report_native_app_store_memory.py',
                 'scripts/build_portable_updates.py', target.RECIPE)
        for relative in paths:
            with self.subTest(path=relative):
                self.assertEqual((target.ROOT/relative).read_bytes(),
                                 target.git(target.ROOT, 'show', target.SOURCE_BASELINE+':'+relative))

    def test_app_store_and_firmware_recipes_remain_distinct(self):
        args = app_store.compose_arguments((app_store.ROOT/app_store.RECIPE).read_bytes(), self.repos,
            self.out, self.out/'include', self.out/'profile', self.out/'runtime-sdk', '1.4.0')
        app_store.validate_recipe(args, '1.4.0')
        with self.assertRaises(ValueError):
            target.validate_recipe(args, self.version)
        with self.assertRaises(ValueError):
            app_store.validate_recipe(self.arguments(), '1.4.0')

    def test_zero_entry_catalog_is_retained_production_source(self):
        self.assertEqual((target.ROOT/target.CATALOG_PROFILE).read_bytes(),
                         (target.ROOT/app_store.CATALOG_PROFILE).read_bytes())
        self.assertNotIn('fixtures', Path(target.CATALOG_PROFILE).parts)

    def test_host_compiler_refused(self):
        with self.assertRaisesRegex(ValueError, 'does not emit'):
            target.verify_compiler('cc', self.out)

    def test_stale_output_refused_before_any_build(self):
        (self.out/'stale').write_text('stale')
        args = target.argument_parser().parse_args([
            *[argument for key, path in self.repos.items()
              for argument in ('--'+key.replace('_', '-'), str(path))],
            '--output-dir', str(self.out)])
        with self.assertRaisesRegex(ValueError, 'empty output directory'):
            target.build(args)

    def make_binding(self):
        elf = self.out/'ota_update.elf'
        elf.write_bytes(b'unit-test image identity; structural checks are separate')
        manifest = self.out/'ota_update.json'
        manifest.write_bytes((target.ROOT/target.APP_MANIFEST).read_bytes())
        layout = self.out/'elf-layout.json'
        layout.write_text('{}')
        command = self.out/'command.json'
        command.write_text(json.dumps(self.arguments()))
        files = (target.VERSION_PROFILE, target.APP_MANIFEST, target.RECIPE, target.CATALOG_PROFILE,
                 'scripts/build_native_firmware_update_qualification.py', 'scripts/native_toolchain.py',
                 'scripts/report_native_settings_memory.py', 'Apps/ota_update.c',
                 'Apps/update_portable.inc', 'Apps/update_native.inc',
                 'lib/PortableApps/src/adapter.c', 'lib/PortableApps/include/SoftwareUpdateV1.h')
        record = {'status': 'PASS', 'profile': 'native-firmware-update',
                  'sha256': admission.shared.sha(elf.read_bytes()), 'size_bytes': elf.stat().st_size,
                  'manifest_sha256': admission.shared.sha(manifest.read_bytes()),
                  'elf_layout_sha256': admission.shared.sha(layout.read_bytes()),
                  'command_sha256': admission.shared.sha(command.read_bytes()),
                  'source_baseline_commit': target.SOURCE_BASELINE,
                  'repository_commit': target.git(target.ROOT, 'rev-parse', 'HEAD').decode().strip(),
                  'external_sources': {key: {'repository': target.REPOSITORIES[key], 'commit': pin}
                                       for key, pin in target.PINS.items()},
                  'source_sha256': {'system:'+path: admission.shared.sha((target.ROOT/path).read_bytes())
                                    for path in files},
                  'service_contract_sha256': target.service_contract()}
        receipt = self.out/'firmware-update-build-record.json'
        receipt.write_text(json.dumps(record))
        return (elf, manifest, layout, receipt), record

    def test_admission_binding_accepts_matching_inputs(self):
        paths, record = self.make_binding()
        observed, digest = admission.source_binding(*paths)
        self.assertEqual(observed, record)
        self.assertEqual(digest, admission.shared.sha(paths[-1].read_bytes()))

    def test_admission_binding_rejects_each_changed_artifact(self):
        paths, _ = self.make_binding()
        for path in (*paths[:3], self.out/'command.json'):
            content = path.read_bytes()
            path.write_bytes(content+b'changed')
            with self.subTest(path=path.name), self.assertRaises(ValueError):
                admission.source_binding(*paths)
            path.write_bytes(content)

    def test_admission_binding_rejects_stale_incomplete_and_mismatched_receipt(self):
        paths, record = self.make_binding()
        for key, value in (('status', 'FAIL'), ('profile', 'native-app-store'),
                           ('sha256', '0'*64), ('size_bytes', 0),
                           ('manifest_sha256', '0'*64), ('elf_layout_sha256', '0'*64),
                           ('command_sha256', '0'*64), ('source_baseline_commit', '0'*40),
                           ('repository_commit', '0'*40), ('external_sources', {}),
                           ('source_sha256', {}), ('service_contract_sha256', {})):
            paths[-1].write_text(json.dumps({**record, key: value}))
            with self.subTest(field=key), self.assertRaises(ValueError):
                admission.source_binding(*paths)

    def test_admission_binding_rejects_each_missing_required_source(self):
        paths, record = self.make_binding()
        for identity in record['source_sha256']:
            changed = {**record, 'source_sha256': {k: v for k, v in record['source_sha256'].items()
                                                  if k != identity}}
            paths[-1].write_text(json.dumps(changed))
            with self.subTest(source=identity), self.assertRaises(ValueError):
                admission.source_binding(*paths)

    def test_admission_binding_rejects_changed_source_hashes_and_path_traversal(self):
        paths, record = self.make_binding()
        for identity in ('system:Apps/ota_update.c', 'system:../outside', 'system:/tmp/outside',
                         'unknown:Apps/ota_update.c', 'system:', 'malformed'):
            changed = {**record, 'source_sha256': {**record['source_sha256'], identity: '0'*64}}
            paths[-1].write_text(json.dumps(changed))
            with self.subTest(source=identity), self.assertRaises(ValueError):
                admission.source_binding(*paths)

    def test_admission_layout_rejects_non_xtensa_artifact(self):
        paths, _ = self.make_binding()
        with self.assertRaises(ValueError):
            admission.matching_layout(paths[0], paths[2])


if __name__ == '__main__':
    unittest.main(verbosity=2)
