import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from timezone_settings_witness import (FEATURE, FEATURE_FILES, INTEGRATION, POISON,
    assert_identical, assert_timezone_absent, erase_timezone, prepare_projection)


class TimezoneSettingsWitness(unittest.TestCase):
    def test_erasure_retains_else_and_nested_unrelated_gates(self):
        source = ('before\n#ifdef OTHER\n#ifdef '+FEATURE+'\n'
                  '#if NESTED\nfeature\n#else\nfeature_else\n#endif\n'
                  '#else\n#if KEEP\nfallback\n#endif\n#endif\n#endif\nafter\n')
        erased, count = erase_timezone(source)
        self.assertEqual(count, 1)
        self.assertEqual(erased, 'before\n#ifdef OTHER\n#if KEEP\nfallback\n#endif\n#endif\nafter\n')

    def test_unknown_gate_and_malformed_input_fail_closed(self):
        for source in ('ordinary\n', '#ifdef '+FEATURE+'\n', '#endif\n',
                       '#if defined('+FEATURE+')\n#endif\n',
                       '#ifndef '+FEATURE+'\n#endif\n',
                       '#if OTHER\n#elif defined('+FEATURE+')\n#endif\n',
                       '#ifdef '+FEATURE+'\n#elif OTHER\n#endif\n',
                       '#ifdef '+FEATURE+'\n#else\n#else\n#endif\n'):
            with self.subTest(source=source), self.assertRaises(ValueError):
                erase_timezone(source)

    def test_projection_changes_only_named_feature_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for path in (*INTEGRATION, *FEATURE_FILES):
                target = root/path
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT/path, target)
            ordinary = root/'ordinary.c'
            ordinary.write_text('preserve exactly\n')
            counts = prepare_projection(root)
            self.assertTrue(all(counts[path] > 0 for path in INTEGRATION))
            self.assertEqual(ordinary.read_text(), 'preserve exactly\n')
            for path in FEATURE_FILES:
                self.assertEqual((root/path).read_text(), '#error '+POISON+'\n')
            for path in INTEGRATION:
                self.assertNotIn('#ifdef '+FEATURE, (root/path).read_text())

    def test_poison_only_keeps_gate_reachable(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for path in (*INTEGRATION, *FEATURE_FILES):
                target = root/path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('#ifdef '+FEATURE+'\nint optional;\n#endif\n')
            original = (root/INTEGRATION[0]).read_bytes()
            self.assertEqual(prepare_projection(root, poison_only=True), {})
            self.assertEqual((root/INTEGRATION[0]).read_bytes(), original)

    def test_leaked_selector_and_mismatched_bytes_are_rejected(self):
        for identifier in ('stz_activate', 'portable_timezone_preference_load', 'SV_TIMEZONE_CITIES'):
            with self.subTest(identifier=identifier), self.assertRaises(AssertionError):
                assert_timezone_absent('int '+identifier+';', 'mutation')
        assert_timezone_absent('portable_time_zone();', 'preserved legacy timezone')
        with self.assertRaises(AssertionError):
            assert_identical(b'current', b'changed', 'mutation')
        assert_identical(b'current', b'current', 'same')

    def test_compiler_poison_detects_unguarded_include_mutation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            (root/'optional.h').write_text('#error '+POISON+'\n')
            source = root/'fixture.c'
            source.write_text('#ifdef '+FEATURE+'\n#include "optional.h"\n#endif\nint ordinary;\n')
            command = ['cc', '-E', '-P', str(source)]
            off = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(off.returncode, 0, off.stderr)
            on = subprocess.run([*command, '-D'+FEATURE], capture_output=True, text=True)
            self.assertNotEqual(on.returncode, 0)
            self.assertIn(POISON, on.stderr)
            source.write_text('#include "optional.h"\nint ordinary;\n')
            leaked = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(leaked.returncode, 0)
            self.assertIn(POISON, leaked.stderr)


if __name__ == '__main__':
    unittest.main()
