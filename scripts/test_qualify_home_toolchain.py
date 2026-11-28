"""Modeled subprocess outcomes only; run in the authorized test environment."""
import contextlib
import io
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT=Path(__file__).with_name('qualify_home_toolchain.py')

class Receipts(unittest.TestCase):
 def scenario(self,mode):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);cc=root/'xtensa-esp32s3-elf-gcc';cc.write_bytes(b'modeled compiler')
   out=root/'out';calls=[]
   def run(argv,**kwargs):
    current=json.loads((out/'receipt.json').read_text())
    state=current['attempts'][-1]['state']
    self.assertIn(state,('initial-invocation','repeat-invocation'))
    calls.append(argv)
    if mode=='exception':raise OSError('modeled launch failure')
    dest=Path(argv[-1]);repeat=dest.name=='repeat'
    failed=mode=='initial' or (mode=='repeat' and repeat) or (mode=='eventual' and dest.parent.name=='no-ivopts')
    if not failed:
     (dest/'home').mkdir(parents=True)
     (dest/'home/default.elf').write_bytes(b'different' if mode=='mismatch' and repeat else b'modeled ELF')
    return subprocess.CompletedProcess(argv,1 if failed else 0,'modeled log\n')
   argv=[str(SCRIPT)]
   for name,value in [('x4',root),('runtime',root),('reader',root),('utilities',root),('compiler',cc),('output',out)]:argv+=['--'+name,str(value)]
   error=None
   with patch.object(sys,'argv',argv),patch('subprocess.run',side_effect=run),patch('subprocess.check_output',return_value='modeled version\n'),contextlib.redirect_stdout(io.StringIO()):
    try:runpy.run_path(str(SCRIPT),run_name='__main__')
    except (AssertionError,subprocess.CalledProcessError,OSError) as caught:error=caught
   receipt=json.loads((out/'receipt.json').read_text())
   self.assertFalse(receipt['hardware_tested'])
   successful=mode in ('success','eventual')
   self.assertEqual(receipt['state'],'passed' if successful else 'failed')
   self.assertEqual(error is None,successful)
   return receipt,calls
 def test_all_initial_failures(self):
  r,c=self.scenario('initial');self.assertEqual(len(c),6);self.assertTrue(all(a['returncode']==1 for a in r['attempts']));self.assertIsNone(r['selected'])
 def test_failed_repeat(self):
  r,c=self.scenario('repeat');self.assertEqual(len(c),2);self.assertEqual(r['attempts'][0]['repeat_returncode'],1);self.assertEqual(r['failure']['type'],'CalledProcessError')
  self.assertEqual(r['attempts'][0]['returncode'],0);self.assertEqual(r['failure']['attempt_state'],'repeat-returned')
 def test_mismatch(self):
  r,c=self.scenario('mismatch');self.assertEqual(len(c),2);self.assertFalse(r['attempts'][0]['repeat_identical']);self.assertEqual(r['failure']['attempt_state'],'comparing')
  self.assertEqual(r['attempts'][0]['returncode'],0);self.assertEqual(r['attempts'][0]['repeat_returncode'],0)
 def test_first_variant_success(self):
  r,c=self.scenario('success');self.assertEqual(len(c),2);self.assertEqual(r['selected']['label'],'no-ivopts');self.assertTrue(r['selected']['repeat_identical'])
 def test_eventual_success(self):
  r,c=self.scenario('eventual');self.assertEqual(len(c),3);self.assertEqual(r['selected']['label'],'no-switch-tables');self.assertTrue(r['selected']['repeat_identical'])
 def test_launch_exception(self):
  r,c=self.scenario('exception');self.assertEqual(len(c),1);self.assertIsNone(r['attempts'][0]['returncode']);self.assertEqual(r['failure']['attempt_state'],'initial-invocation')

if __name__=='__main__':unittest.main()
