"""A piped qualification failure must stop CI before publishing artifacts."""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeOverlayWorkflow(unittest.TestCase):
    def test_negative_pipeline_control(self):
        script = 'false | tee /dev/null'
        self.assertEqual(subprocess.run(['bash', '-e', '-c', script]).returncode, 0)
        self.assertEqual(subprocess.run(['bash', '--noprofile', '--norc', '-eo',
                                        'pipefail', '-c', script]).returncode, 1)

    def test_all_native_qualification_jobs_have_explicit_bash(self):
        for app, jobs in (('settings', 1), ('wifi', 2)):
            with self.subTest(app=app):
                source = (ROOT / ('.github/workflows/native-' + app + '.yml')).read_text()
                defaults = re.findall(r'^    defaults:\n      run:\n((?:        [^\n]+\n)+)',
                                      source, re.M)
                self.assertEqual(len(defaults), jobs)
                self.assertTrue(all('        shell: bash\n' in block for block in defaults))
                self.assertIn('python scripts/native_overlay_evidence.py --app ' + app, source)


if __name__ == '__main__':
    unittest.main()
