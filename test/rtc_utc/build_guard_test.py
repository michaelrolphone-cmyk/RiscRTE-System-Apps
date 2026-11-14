import pathlib,tempfile,json,subprocess,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class Guard(unittest.TestCase):
 def check(self,change,offset='28800',version='1.2.3',expected='Expected the original'):
  with tempfile.TemporaryDirectory() as t:
   r=pathlib.Path(t);m={'type':'driver','driver_abi':2,'architecture':'xtensa-esp32s3','provides':[{'capability':'rtc.clock','api':2}],'file_name':'driver.elf'};m.update(change);(r/'manifest.json').write_text(json.dumps(m))
   c=[sys.executable,str(ROOT/'scripts/build_rtc_utc_provider.py'),'--source',__file__,'--manifest',str(r/'manifest.json'),'--runtime',str(r),'--output',str(r/'output'),'--offset-seconds',offset,'--version',version]
   p=subprocess.run(c,text=True,capture_output=True);self.assertNotEqual(p.returncode,0);self.assertIn(expected,p.stderr);self.assertFalse((r/'output').exists())
 def test_high_offset(self):self.check({},'86401',expected='Explicit RTC basis')
 def test_low_offset(self):self.check({},'-86401',expected='Explicit RTC basis')
 def test_version(self):self.check({},version='draft',expected='Expected explicit package')
 def test_type(self):self.check({'type':'application'})
 def test_abi(self):self.check({'driver_abi':1})
 def test_architecture(self):self.check({'architecture':'host'})
 def test_extra_capability(self):self.check({'provides':[{'capability':'rtc.clock','api':2},{'capability':'other','api':1}]})
 def test_wrong_api(self):self.check({'provides':[{'capability':'rtc.clock','api':1}]})
 def test_filename(self):self.check({'file_name':'../driver.elf'})
if __name__=='__main__':unittest.main()
