"""Run: python3 docs/shared-app-crashes/replay/test_checked_xtensa_link.py"""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import checked_xtensa_link as link

SIGNATURE = (b'collect2: fatal error: ld terminated with signal 6 [Aborted], core dumped\n'
             b'compilation terminated.\ndouble free or corruption (out)\n')


class CheckedLinkTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.target = self.root / 'app.elf'
        self.log = self.root / 'app.log'
        self.command = ['gcc', '-flto', 'source.c', '-o', str(self.target)]
        self.calls = []

    def run_results(self, results, attempts=6):
        responses = iter(results)
        def run(command, **kwargs):
            self.assertFalse(self.target.exists(), 'stale or failed ELF survived into compiler attempt')
            self.assertEqual(tuple(self.command), command)
            self.assertEqual(kwargs, {'stdout': subprocess.PIPE, 'stderr': subprocess.STDOUT})
            self.calls.append(command)
            status, output, artifact = next(responses)
            if artifact is not None:
                self.target.write_bytes(artifact)
            return subprocess.CompletedProcess(command, status, output)
        with patch.object(link.subprocess, 'run', side_effect=run):
            return link.compile_checked(self.command, self.target, self.log, max_attempts=attempts)

    def test_first_success_has_no_retry(self):
        rows = self.run_results([(0, b'', b'valid')])
        self.assertEqual(len(rows), 1)
        self.assertEqual(self.target.read_bytes(), b'valid')
        self.assertFalse(rows[0]['recognized_linker_abort'])

    def test_exact_abort_then_success_preserves_command_logs_and_cleans_stale_output(self):
        self.target.write_bytes(b'stale prior ELF')
        rows = self.run_results([(1, SIGNATURE, b'failed partial ELF'), (0, b'', b'valid')])
        self.assertEqual(self.calls, [tuple(self.command)] * 2)
        self.assertTrue(rows[0]['recognized_linker_abort'])
        self.assertEqual(Path(rows[0]['log']).read_bytes(), SIGNATURE)
        self.assertEqual(Path(rows[1]['log']).read_bytes(), b'')
        self.assertEqual(self.target.read_bytes(), b'valid')
        receipt = json.loads((self.root / 'app-link-attempts/receipt.json').read_text())
        self.assertEqual(receipt['command'], self.command)
        self.assertEqual(receipt['attempts'], rows)

    def test_signature_without_core_dump_suffix(self):
        self.assertTrue(link.recognized_linker_abort(SIGNATURE.replace(b', core dumped', b'')))

    def test_ordinary_failure_is_not_retried_and_output_is_removed(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_results([(1, b'undefined reference to missing_symbol\n', b'partial')])
        self.assertEqual(len(self.calls), 1)
        self.assertFalse(self.target.exists())
        self.assertEqual(self.log.read_bytes(), b'undefined reference to missing_symbol\n')

    def test_each_partial_signature_is_rejected(self):
        lines = SIGNATURE.splitlines(keepends=True)
        for index in range(len(lines)):
            with self.subTest(missing=index):
                self.assertFalse(link.recognized_linker_abort(b''.join(lines[:index] + lines[index + 1:])))
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_results([(1, lines[0] + lines[2], b'partial')])
        self.assertEqual(len(self.calls), 1)

    def test_signature_with_additional_error_is_rejected(self):
        self.assertFalse(link.recognized_linker_abort(SIGNATURE + b'undefined reference to missing_symbol\n'))

    def test_exhaustion_is_bounded_and_keeps_all_failed_logs(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_results([(1, SIGNATURE, b'partial')] * 3, attempts=3)
        self.assertEqual(len(self.calls), 3)
        self.assertFalse(self.target.exists())
        logs = sorted((self.root / 'app-link-attempts').glob('attempt-*.log'))
        self.assertEqual(len(logs), 3)
        self.assertTrue(all(p.read_bytes() == SIGNATURE for p in logs))

    def test_success_cannot_reuse_an_old_elf(self):
        self.target.write_bytes(b'old')
        with self.assertRaisesRegex(RuntimeError, 'without creating'):
            self.run_results([(0, b'', None)])
        self.assertFalse(self.target.exists())

    def test_previous_attempt_logs_are_never_overwritten(self):
        old = self.root / 'app-link-attempts'
        old.mkdir()
        prior = old / 'attempt-01.log'
        prior.write_bytes(b'previous failure')
        rows = self.run_results([(0, b'', b'valid')])
        self.assertEqual(prior.read_bytes(), b'previous failure')
        self.assertEqual(Path(rows[0]['log']).parent.name, 'app-link-attempts-2')

    def test_invalid_attempt_bound_fails_before_invoking_compiler(self):
        for count in [0, 7]:
            with self.subTest(count=count), self.assertRaises(ValueError):
                link.compile_checked(self.command, self.target, self.log, max_attempts=count)


if __name__ == '__main__':
    unittest.main(verbosity=2)
