#!/usr/bin/env python3
"""Offline USB profile selection, dependency and negative-control tests."""
import argparse
from contextlib import redirect_stderr
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import build_portable_usb_transfer as usb
import portable_quick_build as shared


class UsbNativeProfile(unittest.TestCase):
    def arguments(self, *flags):
        return usb.argument_parser().parse_args(['--msc-sdk', '/unused', *flags])

    def test_unselected_defaults_are_unchanged(self):
        args = self.arguments()
        with tempfile.TemporaryDirectory() as directory:
            flags = usb.validate_selection(args, Path(directory))
        self.assertEqual(flags, usb.FLAGS)
        self.assertFalse(args.native_ui)
        self.assertFalse(set(flags) & set(usb.NATIVE_FLAGS))

    def test_legacy_caller_without_selector_is_unselected(self):
        args = self.arguments()
        del args.native_ui
        self.assertEqual(usb.validate_selection(args), usb.FLAGS)

    def test_native_adds_exact_required_selectors(self):
        for touch in (False, True):
            args = self.arguments('--native-ui', *(['--touch-only'] if touch else []))
            flags = usb.validate_selection(args)
            self.assertEqual(flags, [f for f in usb.FLAGS if not touch or f != '-DPORTABLE_INPUT_NAVIGATION']+usb.NATIVE_FLAGS)
            self.assertEqual(len(flags), len(set(flags)))

    def test_each_native_source_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in usb.NATIVE_SOURCES:
                path = root/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            for name in usb.NATIVE_SOURCES:
                path = root/name
                path.unlink()
                with self.subTest(missing=name), self.assertRaisesRegex(ValueError, 'requires the USB native implementation'):
                    usb.validate_selection(self.arguments('--native-ui'), root)
                path.write_text(name)

    def test_receipt_is_source_bound_and_claims_no_product_version(self):
        hashes = {name: usb.sha(usb.ROOT/name) for name in usb.NATIVE_SOURCES}
        receipt = usb.native_receipt(hashes)
        self.assertEqual(receipt['source_sha256'], hashes)
        self.assertEqual(receipt['baseline_version'], '0.1.11')
        self.assertTrue(receipt['selected'] and receipt['source_only'])
        for flag in ('hardware_verified', 'product_bound', 'released_artifact', 'version_claim', 'grants_added'):
            self.assertIs(receipt[flag], False)
        self.assertEqual(receipt['minimum_rgb'], [160, 160])

    def test_sdk_missing_or_modified_is_rejected_before_compiler(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for data in (None, b'not the canonical SDK\n'):
                if data is not None:
                    (root/usb.MSC_HEADER).write_bytes(data)
                args = self.arguments('--native-ui', '--msc-sdk', str(root), '--output-dir', str(root/'build'))
                with patch.object(usb, 'resolve_compiler') as compiler, self.assertRaises(ValueError):
                    usb.build(args)
                compiler.assert_not_called()
                self.assertFalse((root/'build').exists())

    def test_native_pin_is_not_imposed_on_legacy_sdk(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory)/usb.MSC_HEADER
            header.write_text('legacy SDK')
            usb.validate_sdk(header, False)
            with self.assertRaisesRegex(ValueError, 'pinned complete MSC'):
                usb.validate_sdk(header, True)

    def test_quick_power_and_host_options_are_rejected_even_without_native(self):
        paths = {'x4_idle_source', 'x4_idle_sdk', 'x4_idle_runtime_sdk', 'frontlight_tone_sdk', 'display_settled_sdk'}
        for native in (False, True):
            for name in (*usb.FORBIDDEN_OPTIONS, 'resident_shell_host'):
                option = ['--'+name.replace('_', '-')]
                if name in paths:
                    option.append('/unused')
                with self.subTest(native=native, option=name), self.assertRaises(ValueError):
                    usb.validate_selection(self.arguments(*(['--native-ui'] if native else []), *option))

    def test_resident_sdk_options_require_resident_role(self):
        for option in (('--resident-runtime-sdk', '/unused'), ('--resident-policy',), ('--tagged-alarm-utilities', '/unused')):
            with self.subTest(option=option), self.assertRaisesRegex(ValueError, 'requires --resident-shell-client'):
                usb.validate_selection(self.arguments(*option))
        with self.assertRaisesRegex(ValueError, 'canonical tagged alarm SDK'):
            usb.validate_selection(self.arguments('--resident-shell-client'))

    def test_native_resident_scope_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'qualified only for standalone foreground'):
            usb.validate_selection(self.arguments('--native-ui', '--resident-shell-client',
                '--tagged-alarm-utilities', '/unused', '--resident-runtime-sdk', '/unused'))

    def test_alarm_native_requires_both_profile_and_role(self):
        for flags in ((), ('--native-ui',), ('--resident-shell-client', '--tagged-alarm-utilities', '/unused')):
            with self.subTest(flags=flags), self.assertRaisesRegex(ValueError, '--alarm-native requires'):
                usb.validate_selection(self.arguments('--alarm-native', *flags))

    def test_snapshot_is_explicit_and_independent_of_native(self):
        for native in (False, True):
            args = self.arguments('--raster-snapshot', *(['--native-ui'] if native else []))
            args.alarm_client = False
            flags = usb.validate_selection(args)
            with tempfile.TemporaryDirectory() as directory:
                selected, sources = shared.configure(args, usb.argument_parser(), usb.ROOT, Path(directory))
            self.assertEqual(selected, ['-DPORTABLE_RASTER_SNAPSHOT'])
            self.assertEqual(sources, [])
            self.assertEqual('-DPORTABLE_USB_TRANSFER_NATIVE_UI' in flags, native)

    def test_home_override_removes_default_before_shared_selection(self):
        args = self.arguments('--home-app', 'home.elf')
        args.alarm_client = False
        flags = usb.validate_selection(args)
        with tempfile.TemporaryDirectory() as directory:
            selected, sources = shared.configure(args, usb.argument_parser(), usb.ROOT, Path(directory))
        self.assertEqual([f for f in flags+selected if f.startswith('-DPORTABLE_HOME_APP=')], ['-DPORTABLE_HOME_APP="home.elf"'])
        self.assertFalse(sources)

    def test_native_does_not_change_manifest_identity_or_permissions(self):
        manifest = json.loads((usb.ROOT/'Apps/native/usb_sd_transfer.json').read_text())
        self.assertEqual(manifest['version'], '0.1.11')
        self.assertEqual(manifest['id'], 'usb_sd_transfer')
        self.assertEqual({r['capability'] for r in manifest['requires']},
            {'display.output', 'input.touch.raw', 'input.navigation', 'usb.device.msc'})
        for native in (False, True):
            args = self.arguments(*(['--native-ui'] if native else []))
            self.assertEqual(shared.version(args, 'usb_sd_transfer', manifest['version']), '0.1.11')
            self.assertEqual(shared.exports(args, {'app_main', 'app_module_init', 'app_module_fini'}),
                {'app_main', 'app_module_init', 'app_module_fini'})

    def test_native_header_rejects_each_missing_compile_dependency(self):
        import os
        required = ['PORTABLE_USB_TRANSFER_APP', 'PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS']
        for missing in required:
            flags = ['-DPORTABLE_USB_TRANSFER_NATIVE_UI']+['-D'+name for name in required if name != missing]
            result = subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-fsyntax-only',
                '-x', 'c', *flags, '-I'+str(usb.ROOT/'Apps'), '-'],
                input='#include "UsbTransferNative.h"\n', text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0, missing)
            self.assertIn('USB native UI requires', result.stderr)


if __name__ == '__main__':
    unittest.main()
