"""Real Git witnesses for the immutable inputs shared by memory reporters."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import report_native_settings_memory as reporter


class PinnedMemorySource(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Source fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.path = 'lib/loader.c'
        (self.root / 'lib').mkdir()
        (self.root / self.path).write_bytes(b'original pinned loader\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'original')
        self.pin = self.git('rev-parse', 'HEAD').decode().strip()
        self.original_blob = self.git('rev-parse', self.pin + ':' + self.path).decode().strip()
        self.original_tree = self.git('rev-parse', self.pin + '^{tree}').decode().strip()
        (self.root / self.path).write_bytes(b'substituted loader\n')
        self.git('commit', '-qam', 'substitution')
        self.replacement = self.git('rev-parse', 'HEAD').decode().strip()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args])

    def read(self):
        return reporter.pinned_runtime_input(self.root, self.pin, self.path)

    def test_reads_exact_pin_not_head_or_worktree(self):
        (self.root / self.path).write_bytes(b'uncommitted loader\n')
        self.assertEqual(self.read(), b'original pinned loader\n')

    def test_commit_replacement_does_not_substitute_source(self):
        self.git('replace', self.pin, self.replacement)
        self.assertEqual(self.git('show', self.pin + ':' + self.path), b'substituted loader\n')
        self.assertEqual(self.read(), b'original pinned loader\n')

    def test_tree_replacement_does_not_substitute_source(self):
        tree = self.git('rev-parse', self.replacement + '^{tree}').decode().strip()
        self.git('replace', self.original_tree, tree)
        self.assertEqual(self.git('show', self.pin + ':' + self.path), b'substituted loader\n')
        self.assertEqual(self.read(), b'original pinned loader\n')

    def test_blob_replacement_does_not_substitute_source(self):
        blob = self.git('rev-parse', self.replacement + ':' + self.path).decode().strip()
        self.git('replace', self.original_blob, blob)
        self.assertEqual(self.git('show', self.pin + ':' + self.path), b'substituted loader\n')
        self.assertEqual(self.read(), b'original pinned loader\n')

    def test_custom_replacement_namespace_is_not_followed(self):
        namespace = 'refs/fixture-replacements/'
        self.git('update-ref', namespace + self.pin, self.replacement)
        with patch.dict(os.environ, {'GIT_REPLACE_REF_BASE': namespace}):
            self.assertEqual(self.git('show', self.pin + ':' + self.path), b'substituted loader\n')
            self.assertEqual(self.read(), b'original pinned loader\n')

    def test_ambient_git_directory_cannot_redirect_input(self):
        with tempfile.TemporaryDirectory() as other:
            subprocess.run(['git', 'init', '-q', other], check=True)
            with patch.dict(os.environ, {'GIT_DIR': str(Path(other) / '.git'),
                                        'GIT_WORK_TREE': other}):
                self.assertEqual(self.read(), b'original pinned loader\n')

    def test_missing_pin_is_not_replaced_by_worktree(self):
        with self.assertRaises(subprocess.CalledProcessError):
            reporter.pinned_runtime_input(self.root, '0' * 40, self.path)

    def test_non_git_environment_is_preserved(self):
        original = subprocess.check_output
        environments = []
        def capture(*args, **kwargs):
            environments.append(kwargs['env'])
            return original(*args, **kwargs)
        with patch.dict(os.environ, {'MEMORY_SOURCE_TEST': 'retained', 'GIT_TRACE': '1'}), \
             patch.object(reporter.subprocess, 'check_output', side_effect=capture):
            self.assertEqual(self.read(), b'original pinned loader\n')
        self.assertEqual(environments[0]['MEMORY_SOURCE_TEST'], 'retained')
        self.assertFalse(any(key.startswith('GIT_') for key in environments[0]))


if __name__ == '__main__':
    unittest.main()
