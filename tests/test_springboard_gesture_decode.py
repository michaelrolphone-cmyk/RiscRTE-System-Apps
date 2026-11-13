import importlib.util,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('gesture_decoder',ROOT/'scripts/decode_springboard_gestures.py');decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
def word(tag,slot,kind,x,y):return(tag<<24)|(slot<<22)|(kind<<20)|((x&1023)<<10)|(y&1023)
def words():
 result=[(0xe0<<24)|(1<<21)|(465<<10)|465,(0xe1<<24)|(15<<8)|7]
 for slot,(x,y,dx,dy) in enumerate([(233,233,0,0),(234,233,1,0),(232,233,-1,0),(153,233,0,0)]):
  kind=0 if slot==0 else 2 if slot==3 else 1
  result +=[word(0xe2,slot,kind,x,y),word(0xe3,slot,kind,x,y),word(0xe4,slot,kind,dx,dy)]
 return result
def lines(values):return[f'RTE_PERF seq={i+1} us={9000+i} interaction=4 invocation=9 phase=8 value={v}\n' for i,v in enumerate(values)]
class GestureDecoder(unittest.TestCase):
 def test_complete(self):
  result=decoder.decode(lines(words()));self.assertEqual(len(result['gestures']),1);self.assertEqual(result['gestures'][0]['points'][2]['dispatched_delta'],[-1,0]);self.assertFalse(result['physical_timestamp']);self.assertFalse(result['raw_CST_bytes']);self.assertFalse(result['metrics'])
 def test_incomplete(self):self.assertEqual(decoder.decode(lines(words()[:-1]))['gestures'],[])
 def test_gap(self):
  data=lines(words());data[4]=data[4].replace('seq=5','seq=99');self.assertEqual(decoder.decode(data)['gestures'],[])
 def test_bad_geometry(self):
  data=words();data[2]=word(0xe2,0,0,466,0)
  with self.assertRaises(ValueError):decoder.decode(lines(data))
 def test_bad_delta(self):
  data=words();data[7]=word(0xe4,1,1,300,0)
  with self.assertRaises(ValueError):decoder.decode(lines(data))
 def test_ignore_metrics(self):self.assertEqual(decoder.decode(lines([0x01000020,0x02000100]))['gestures'],[])
 def test_schema(self):
  data=words();data[0]^=3<<21
  with self.assertRaises(ValueError):decoder.decode(lines(data))
 def test_slot_mismatch(self):
  data=words();data[3]=word(0xe3,2,1,233,233)
  with self.assertRaises(ValueError):decoder.decode(lines(data))
if __name__=='__main__':unittest.main()
