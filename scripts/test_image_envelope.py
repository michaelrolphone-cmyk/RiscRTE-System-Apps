#!/usr/bin/env python3
import random,struct,unittest
from image_envelope import *
class Envelope(unittest.TestCase):
 def raw(self,n=8192):
  random.seed(42);return b'\x7fELF'+bytes(random.randrange(8) for _ in range(n-4))
 def test_exact_deterministic_roundtrip(self):
  for n in (52,256,4096,65536,131072):
   raw=self.raw(n);packed=encode_image(raw)
   self.assertEqual(encode_image(raw),packed);self.assertEqual(decode_image(packed),raw)
   self.assertEqual(receipt(raw,packed)['canonical_bytes'],n)
 def test_header_and_crc_refusals(self):
  good=encode_image(self.raw());fields=list(HEADER.unpack_from(good))
  for field,value in ((0,b'UNKNOWN!'),(1,31),(2,2),(3,51),(3,MAX_IMAGE+1),(4,0),(4,fields[4]+1),(5,fields[5]^1),(6,1)):
   bad=fields.copy();bad[field]=value
   with self.subTest(field=field,value=value),self.assertRaises(ValueError):decode_image(HEADER.pack(*bad)+good[HEADER.size:])
  for n in (0,1,7,31,len(good)-1):
   with self.subTest(n=n),self.assertRaises(ValueError):decode_image(good[:n])
  with self.assertRaises(ValueError):decode_image(good+b'extra')
  with self.assertRaises(ValueError):decode_image(good,maximum=100)
 def test_input_refusals(self):
  for raw in (b'',b'\x7fELF',b'x'*52,b'\x7fELF'+b'x'*MAX_IMAGE):
   with self.assertRaises(ValueError):encode_image(raw)
 def test_corrupt_payload_and_distance(self):
  good=encode_image(self.raw());bad=bytearray(good);bad[-1]^=1
  with self.assertRaises(ValueError):decode_image(bytes(bad))
  for block,size in ((b'\x00\x00\x00',4),(b'\x00\x01\x00',4),(b'\xf0',52),(b'\xff\xff',52)):
   with self.assertRaises(ValueError):decode(block,size)
if __name__=='__main__':unittest.main()
