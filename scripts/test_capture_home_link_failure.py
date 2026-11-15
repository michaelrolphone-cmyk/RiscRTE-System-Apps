import importlib.util
from pathlib import Path
import unittest
spec = importlib.util.spec_from_file_location('capture', Path(__file__).with_name('capture_home_link_failure.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class CaptureTests(unittest.TestCase):
    def test_exact_command_preserved(self):
        argv = ['/tmp/target/xtensa-esp32s3-elf-gcc', '-shared', '-DVALUE="a b"', 'source.c', '-o', '/tmp/out/default.elf']
        log = "subprocess.CalledProcessError: Command '" + repr(argv) + "' returned non-zero exit status 1."
        self.assertEqual(module.failed_compiler_command(log), argv)
    def test_outer_python_ignored(self):
        with self.assertRaises(ValueError):
            module.failed_compiler_command("CalledProcessError: Command '['python', 'build.py']' returned non-zero")
    def test_missing_command_rejected(self):
        with self.assertRaises(ValueError): module.failed_compiler_command('ld crashed')
    def test_non_compiler_rejected(self):
        with self.assertRaises(ValueError):
            module.failed_compiler_command("CalledProcessError: Command '['sh', '-shared', '-o', 'x']' returned non-zero")

if __name__ == '__main__': unittest.main()
