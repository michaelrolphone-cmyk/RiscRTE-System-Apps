"""Wi-Fi memory receipt wiring tests; actual Runtime admission runs separately."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import report_native_wifi_memory as reporter


class NativeWifiMemory(unittest.TestCase):
    def fixture(self, directory):
        root = Path(directory)
        elf = root/'wifi_settings.elf'; elf.write_bytes(b'target fixture')
        layout = {'sha256': reporter.shared.sha(elf.read_bytes()), 'file_size_bytes': elf.stat().st_size,
                  'section_loader': {'data_allocation_request_bytes': 1234},
                  'linked_device_memory_fit_qualified': False}
        layout_path = root/'elf-layout.json'; layout_path.write_text(json.dumps(layout))
        (root/'wifi_settings.json').write_text(json.dumps(reporter.expected_manifest('1.4.1')))
        args = reporter.argument_parser().parse_args(['--elf', str(elf), '--runtime', '/runtime',
                                                      '--output', str(root/'out')])
        return args, layout, layout_path

    def test_exact_layout_and_elf_binding_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            args, layout, path = self.fixture(directory)
            with mock.patch.object(reporter.shared, 'inspect', return_value=layout):
                observed, digest = reporter.matching_layout(args.elf, path)
                self.assertEqual(observed, layout)
                self.assertEqual(digest, reporter.shared.sha(path.read_bytes()))
                changed = copy.deepcopy(layout); changed['sha256'] = 'wrong'
                path.write_text(json.dumps(changed))
                with self.assertRaisesRegex(ValueError, 'differs from the target ELF'):
                    reporter.matching_layout(args.elf, path)

    def test_missing_layout_cannot_become_admission_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            args, _, path = self.fixture(directory); path.unlink()
            with self.assertRaises(FileNotFoundError), mock.patch.object(reporter.subprocess, 'run') as run:
                reporter.qualify(args)
            run.assert_not_called()
            self.assertFalse(args.output.exists())

    def test_incomplete_manifest_is_rejected_before_admission(self):
        with tempfile.TemporaryDirectory() as directory:
            args, layout, _ = self.fixture(directory)
            manifest_path = args.elf.parent/'wifi_settings.json'
            manifest = json.loads(manifest_path.read_text()); manifest['requires'].pop()
            manifest_path.write_text(json.dumps(manifest))
            with mock.patch.object(reporter.shared, 'inspect', return_value=layout), \
                 mock.patch.object(reporter.subprocess, 'run') as run:
                with self.assertRaisesRegex(ValueError, 'manifest differs'):
                    reporter.qualify(args)
                run.assert_not_called()
            self.assertFalse(args.output.exists())

    def test_manifest_change_during_admission_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            args, layout, _ = self.fixture(directory)
            def run(command, **kwargs):
                if '-o' not in command:
                    (args.elf.parent/'wifi_settings.json').write_text('{}')
                return self.fake_run(command, **kwargs)
            with mock.patch.object(reporter.shared, 'inspect', return_value=layout), \
                 mock.patch.object(reporter.subprocess, 'check_output', return_value=b'pinned input'), \
                 mock.patch.object(reporter.subprocess, 'run', side_effect=run):
                with self.assertRaisesRegex(ValueError, 'manifest changed'):
                    reporter.qualify(args)
            self.assertFalse((args.output/'elf-memory-admission.json').exists())

    def test_stale_output_is_rejected_before_compilation(self):
        with tempfile.TemporaryDirectory() as directory:
            args, _, _ = self.fixture(directory); args.output.mkdir(); (args.output/'old').touch()
            with mock.patch.object(reporter.subprocess, 'run') as run:
                with self.assertRaisesRegex(ValueError, 'empty'):
                    reporter.qualify(args)
                run.assert_not_called()

    def fake_run(self, command, **kwargs):
        if '-o' in command:
            Path(command[command.index('-o')+1]).touch()
            return subprocess.CompletedProcess(command, 0)
        return subprocess.CompletedProcess(command, 0, json.dumps({
            'ordinary_application_admitted': True, 'data_allocation_request_bytes': 1234,
            'missing_import_rejected': True, 'unsafe_state_rejected': True,
            'driver_role_rejected': True, 'alignment_placements_checked': 64,
            'linked_device_memory_fit_qualified': False}), '')

    def test_pinned_production_inputs_and_sanitized_runs_without_retained_comparison(self):
        with tempfile.TemporaryDirectory() as directory:
            args, layout, path = self.fixture(directory)
            with mock.patch.object(reporter.shared, 'inspect', return_value=layout), \
                 mock.patch.object(reporter.subprocess, 'check_output', return_value=b'pinned input') as read, \
                 mock.patch.object(reporter.subprocess, 'run', side_effect=self.fake_run) as run:
                report = reporter.qualify(args)
            self.assertEqual(read.call_count, len(reporter.RUNTIME_INPUTS))
            self.assertEqual([call.args[0] for call in read.call_args_list], [
                ['git', '--no-replace-objects', '-C', '/runtime', 'show', reporter.RUNTIME_PIN+':'+relative]
                for relative in reporter.RUNTIME_INPUTS])
            self.assertEqual([r['sanitized'] for r in report['admission_runs']], [False, True])
            self.assertEqual(report['images'], {'native': layout})
            self.assertEqual(report['elf_sha256'], layout['sha256'])
            self.assertEqual(report['required_capabilities'], reporter.expected_manifest('1.4.1')['requires'])
            self.assertEqual(report['manifest_sha256'], reporter.shared.sha((args.elf.parent/'wifi_settings.json').read_bytes()))
            self.assertEqual(report['elf_layout_sha256'], reporter.shared.sha(path.read_bytes()))
            self.assertFalse(report['linked_device_memory_fit_qualified'])
            self.assertFalse(any('retained' in key for key in report))
            self.assertIn('-fsanitize=address,undefined', report['admission_runs'][1]['commands'][0])
            self.assertEqual(json.loads((args.output/'elf-memory-admission.json').read_text()), report)
            self.assertTrue((args.output/'native-0.json').is_file())
            self.assertTrue((args.output/'native-1.json').is_file())
            self.assertFalse((args.output/'admit-1').exists())
            self.assertEqual(run.call_count, 6)

    def test_target_change_during_admission_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            args, layout, _ = self.fixture(directory)
            changed = copy.deepcopy(layout); changed['sha256'] = 'changed'
            with mock.patch.object(reporter.shared, 'inspect', side_effect=[layout, changed]), \
                 mock.patch.object(reporter.subprocess, 'check_output', return_value=b'pinned input'), \
                 mock.patch.object(reporter.subprocess, 'run', side_effect=self.fake_run):
                with self.assertRaisesRegex(ValueError, 'differs from the target ELF'):
                    reporter.qualify(args)
            self.assertFalse((args.output/'elf-memory-admission.json').exists())

    def test_production_allocation_disagreement_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            args, layout, _ = self.fixture(directory)
            def run(command, **kwargs):
                result = self.fake_run(command, **kwargs)
                if '-o' not in command:
                    observed = json.loads(result.stdout); observed['data_allocation_request_bytes'] += 1
                    result.stdout = json.dumps(observed)
                return result
            with mock.patch.object(reporter.shared, 'inspect', return_value=layout), \
                 mock.patch.object(reporter.subprocess, 'check_output', return_value=b'pinned input'), \
                 mock.patch.object(reporter.subprocess, 'run', side_effect=run):
                with self.assertRaisesRegex(ValueError, 'allocation helper'):
                    reporter.qualify(args)
            self.assertFalse((args.output/'elf-memory-admission.json').exists())


if __name__ == '__main__':
    unittest.main()
