"""Existing strict linker normalization preserves actionable bytes and refuses drift."""
import importlib.util,pathlib,struct,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('normalizer',ROOT/'scripts/normalize_xtensa_relocations.py');n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
def fixture():
 b=bytearray(512); names=b'\0.shstrtab\0.rela.dyn\0.rela.plt\0.dynamic\0';b[272:272+len(names)]=names
 struct.pack_into('<16sHHIIIIIHHHHHH',b,0,b'\x7fELF\x01\x01\x01'+bytes(9),3,94,1,0,0,64,0,52,0,0,40,5,1)
 rows=[(0,0,0,0,0,0,0,0,0,0),(1,3,0,0,272,len(names),0,0,1,0),(11,4,2,0x2000,320,36,0,0,4,12),(21,4,2,0x2024,356,12,0,0,4,12),(31,6,2,0x2030,368,16,0,0,4,8)]
 for i,r in enumerate(rows):struct.pack_into('<10I',b,64+i*40,*r)
 struct.pack_into('<III',b,320,0x1000,5,0);struct.pack_into('<III',b,356,0x1004,4,0);struct.pack_into('<IIII',b,368,7,0x2000,8,36)
 return b
class Padding(unittest.TestCase):
 def run_image(self,b,expected=None):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'driver.elf';p.write_bytes(b)
   if expected is None:
    with self.assertRaises(ValueError):n.normalize(p)
    self.assertEqual(p.read_bytes(),b);return
   self.assertEqual(n.normalize(p),expected);out=p.read_bytes();self.assertEqual(n.normalize(p),0);return out
 def test_only_two_size_fields_change(self):
  b=fixture();out=self.run_image(b,2);expected=bytearray(b);struct.pack_into('<I',expected,64+2*40+20,12);struct.pack_into('<I',expected,380,12);self.assertEqual(out,expected)
 def test_inconsistent_dynamic_refused(self):
  b=fixture();struct.pack_into('<I',b,380,24);self.run_image(b)
 def test_nontrailing_none_refused(self):
  b=fixture();b[320:332]=bytes(12);struct.pack_into('<III',b,332,0x1000,5,0);self.run_image(b)
 def test_nonzero_unknown_relocation_refused(self):
  b=fixture();struct.pack_into('<I',b,324,99);self.run_image(b)
 def test_nonzero_normalized_gap_refused(self):
  b=bytearray(self.run_image(fixture(),2));b[332]=1;self.run_image(b)
 def test_normalized_dynamic_mismatch_refused(self):
  b=bytearray(self.run_image(fixture(),2));struct.pack_into('<I',b,380,36);self.run_image(b)
 def test_wrong_arch_refused(self):
  b=fixture();struct.pack_into('<H',b,18,3);self.run_image(b)
if __name__=='__main__':unittest.main()
