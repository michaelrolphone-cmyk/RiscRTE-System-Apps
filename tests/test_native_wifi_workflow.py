"""Keep the native Wi-Fi release behind its complete qualification jobs."""
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import build_native_wifi_qualification as builder


class NativeWifiWorkflow(unittest.TestCase):
    def setUp(self):
        self.workflow = (ROOT/'.github/workflows/native-wifi.yml').read_text()

    def test_controller_covers_direct_and_snapshot_normal_and_sanitized(self):
        expected = {'normal': '', 'sanitized': '--sanitize', 'snapshot': '--snapshot',
                    'snapshot-sanitized': '--snapshot --sanitize'}
        modes = dict(re.findall(r'- mode: ([\w-]+)\n\s+flags: \'([^\']*)\'', self.workflow))
        self.assertEqual(modes, expected)
        self.assertIn('python scripts/test_native_wifi.py --output', self.workflow)
        self.assertIn('${{ matrix.flags }}', self.workflow)
        self.assertNotIn('--case ', self.workflow)
        self.assertNotIn('--size ', self.workflow)
        self.assertIn('fail-fast: false', self.workflow)

    def test_exact_external_source_pins_match_builder(self):
        selected = dict(re.findall(r'repository: ([\w./-]+)\n\s+ref: ([0-9a-f]{40})', self.workflow))
        self.assertEqual(selected, {builder.REPOSITORIES[name]: pin for name, pin in builder.PINS.items()})
        self.assertIn('espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5', self.workflow)
        self.assertIn('platformio==6.1.19', self.workflow)
        self.assertIn('NATIVE_APP_CC="$HOME/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc"', self.workflow)

    def test_clean_target_admission_and_artifact_are_required_before_main_only_publish(self):
        controller, qualification = self.workflow.split('  qualify:\n', 1)
        qualify, publish = qualification.split('  publish-development:\n', 1)
        self.assertIn('needs: controller', qualify)
        self.assertIn('needs: qualify', publish)
        self.assertIn("if: github.event_name == 'push' && github.ref == 'refs/heads/main'", publish)
        self.assertNotIn('contents: write', controller+qualify)
        self.assertIn('contents: write', publish)
        self.assertIn("assert not r['working_tree_dirty']", qualify)
        for script in ('build_native_wifi_qualification.py', 'report_native_wifi_memory.py'):
            self.assertIn('python scripts/'+script, qualify)
        self.assertIn('cp build/native-wifi/admission/elf-memory-admission.json build/native-wifi/app/elf-memory-admission.json', qualify)
        for pattern in ('test_native_wifi_*.py', 'test_portable_wifi_contract.py', 'test_native_toolchain.py'):
            self.assertIn(pattern, qualify)
        self.assertIn('source/build/native-wifi/app/wifi_settings.elf', qualify)
        self.assertIn('source/build/native-wifi/app/licenses/', qualify)
        self.assertIn('source/build/native-wifi/**/*.json', qualify)
        self.assertIn('source/build/native-wifi/controller', qualify)
        self.assertIn('python scripts/publish_native_wifi.py --artifact qualified', publish)
        self.assertEqual(qualify.count('native-wifi-software-qualification-'), 1)
        self.assertEqual(publish.count('native-wifi-software-qualification-'), 1)


if __name__ == '__main__':
    unittest.main()
