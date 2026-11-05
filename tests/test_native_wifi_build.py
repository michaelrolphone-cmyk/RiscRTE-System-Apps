"""Native Wi-Fi recipe contracts. Behavioral and target tests run separately."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import build_native_wifi_qualification as builder


class NativeWifiBuild(unittest.TestCase):
    def arguments(self):
        out = Path('/output')
        return builder.compose_arguments((ROOT/builder.RECIPE).read_bytes(),
            {'bridges': Path('/bridges')}, out, out/'include', out/'profile',
            out/'runtime-sdk', '1.4.1')

    def test_separate_native_version_and_exact_full_manifest(self):
        profile = json.loads((ROOT/builder.VERSION_PROFILE).read_text())
        self.assertEqual(profile['version'], '1.4.1')
        self.assertEqual(profile['profile'], 'native-wifi')
        self.assertNotEqual(profile['version'], json.loads((ROOT/'Apps/wifi_settings.json').read_text())['version'])
        self.assertEqual(json.loads((ROOT/builder.APP_MANIFEST).read_text()), builder.manifest('1.4.1'))
        self.assertEqual(len(builder.CAPABILITIES), 18)
        self.assertEqual(set(builder.CAPABILITY_SELECTORS), {cap for cap, _ in builder.CAPABILITIES})
        for capability in ('storage.key-value.atomic', 'storage.key-value.availability',
                           'ui.text-input', 'net.wifi', 'power.sleep', 'motion.accel', 'app.catalog'):
            self.assertIn((capability, 1), builder.CAPABILITIES)

    def test_delivered_recipe_preserves_every_feature_and_source(self):
        arguments = self.arguments()
        original = json.loads((ROOT/builder.RECIPE).read_text())['wifi_settings']['command']
        old = {item for item in original if item.startswith('-D') and not item.startswith('-DPORTABLE_WIFI_VERSION=')}
        self.assertLessEqual(old, set(arguments))
        self.assertEqual({Path(item).name for item in arguments if item.endswith('.c')}, builder.REQUIRED_SOURCES)
        self.assertIn('-DPORTABLE_WIFI_VERSION="1.4.1"', arguments)
        self.assertNotIn('-o', arguments)
        self.assertTrue(all('@' not in item and '/test/' not in item and '/fixtures/' not in item for item in arguments))
        self.assertIn('-I/output/runtime-sdk', arguments)
        self.assertIn('/output/profile/watch-alarm-profile.h', arguments)
        for name in builder.DEFINES:
            self.assertEqual(arguments.count('-D'+name), 1)

    def test_every_required_feature_and_value_is_fail_closed(self):
        arguments = self.arguments()
        for flag in builder.REQUIRED_FEATURES:
            with self.subTest(flag=flag), self.assertRaisesRegex(ValueError, 'lacks'):
                builder.validate_recipe([item for item in arguments if item != '-D'+flag], '1.4.1')
        for name in (*builder.REQUIRED_VALUES, 'PORTABLE_WIFI_VERSION'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'value differs'):
                builder.validate_recipe([item for item in arguments if not item.startswith('-D'+name+'=')], '1.4.1')
        for name in builder.REQUIRED_SOURCES:
            with self.subTest(source=name), self.assertRaisesRegex(ValueError, 'source selection'):
                builder.validate_recipe([item for item in arguments if Path(item).name != name], '1.4.1')
        for extra in ('-DPORTABLE_WIFI_NATIVE_UI=0', '-DPORTABLE_WIFI_NATIVE_UI'):
            with self.subTest(extra=extra), self.assertRaisesRegex(ValueError, 'ambiguous'):
                builder.validate_recipe(arguments+[extra], '1.4.1')
        with self.assertRaisesRegex(ValueError, 'undefined'):
            builder.validate_recipe(arguments+['-UPORTABLE_WIFI_NATIVE_UI'], '1.4.1')

    def test_retained_recipe_bytes_are_immutable(self):
        with self.assertRaisesRegex(ValueError, 'digest changed'):
            builder.compose_arguments((ROOT/builder.RECIPE).read_bytes()+b'\n', {},
                Path('/out'), Path('/include'), Path('/profile'), Path('/sdk'), '1.4.1')

    def test_external_inputs_read_exact_pins(self):
        with mock.patch.object(builder, 'git', return_value=b'pinned') as read:
            self.assertEqual(builder.pinned({'runtime': Path('/runtime')}, 'runtime', 'sdk/app/RiscRuntimeV1.h'), b'pinned')
        read.assert_called_once_with(Path('/runtime'), 'show', builder.PINS['runtime']+':sdk/app/RiscRuntimeV1.h')

    def test_stale_output_is_rejected_before_external_reads(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root/'stale').touch()
            with mock.patch.object(builder, 'git') as read:
                with self.assertRaisesRegex(ValueError, 'empty output'):
                    builder.build(type('Args', (), {'output_dir': root})())
                read.assert_not_called()


if __name__ == '__main__':
    unittest.main()
