import importlib.util,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
import sys
sys.path.insert(0,str(ROOT/'scripts'))
import scene_build
class BuilderTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);self.inc=self.root/'include';self.inc.mkdir();self.source=self.root/'source.c';self.source.write_text('int x;');self.lib=self.root/'support.a';self.lib.write_bytes(b'archive');self.manifest={'type':'driver','file_name':'driver.elf','id':'test','version':'1'};self.commands=[]
 def run_compiler(self,args,check):
  self.commands.append(args);data=bytearray(52);data[:7]=b'\x7fELF\x01\x01\x01';data[16:20]=b'\x03\x00\x5e\x00';Path(args[-1]).write_bytes(data)
 def output(self,args,text):return 'compiler1' if '--version' in args else '0000 T t5_driver_get\n U memset\n'
 def build(self,**kw):
  with patch.object(scene_build.subprocess,'run',self.run_compiler),patch.object(scene_build.subprocess,'check_output',self.output):return scene_build.build('gcc',self.inc,self.root/'out',self.manifest,[self.source],**kw)
 def test_default(self):
  r=self.build();self.assertNotIn('libraries_sha256',r);self.assertNotIn('linker_original_sha256',r);self.assertEqual(self.commands[0][-3:],[str(self.source),'-o',str(self.root/'out/driver.elf')])
 def test_library_after_sources(self):
  r=self.build(libraries=[self.lib]);c=self.commands[0];self.assertEqual(c.index(str(self.lib)),c.index(str(self.source))+1);self.assertIn(str(self.lib),r['libraries_sha256'])
 def test_missing_library_preflight(self):
  with self.assertRaises(FileNotFoundError):self.build(libraries=[self.root/'absent'])
  self.assertFalse((self.root/'out').exists());self.assertEqual(self.commands,[])
 def test_directory_not_library(self):
  with self.assertRaises(FileNotFoundError):self.build(libraries=[self.inc])
 def test_opt_in_normalizer(self):
  with patch('normalize_xtensa_relocations.normalize',return_value=2) as normalize:r=self.build(normalize_relocations=True)
  normalize.assert_called_once();self.assertEqual(r['removed_trailing_none_relocations'],2);self.assertTrue((self.root/'out/driver.elf.linker-original').is_file())
 def test_imports_not_widened(self):
  self.output=lambda args,text:'compiler1' if '--version' in args else '0000 T t5_driver_get\n U forbidden\n'
  with self.assertRaises(ValueError):self.build(libraries=[self.lib])
if __name__=='__main__':unittest.main()
