"""Standalone qualification commands must be inert during unittest discovery."""
import argparse,runpy,subprocess,unittest
from pathlib import Path
from unittest.mock import patch
class QualificationImports(unittest.TestCase):
 def test_command_modules_do_not_parse_or_execute_on_import(self):
  for name in ['test_cst_touch_composition.py','test_last_attempt_runtime.py']:
   with self.subTest(module=name),patch.object(argparse.ArgumentParser,'parse_args',side_effect=AssertionError('argument parsing on import')),patch.object(subprocess,'run',side_effect=AssertionError('command on import')):
    loaded=runpy.run_path(str(Path(__file__).with_name(name)),run_name='qualification_import_probe')
    self.assertTrue(callable(loaded['main']))
if __name__=='__main__':unittest.main()
