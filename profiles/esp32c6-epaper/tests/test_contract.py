import hashlib,json,pathlib,sys,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from catalog import CONTRACT,PROVIDER,PROFILE,INPUT,digest,canonical
from client import Client,read_frame
from provision import validate_config
class ContractTests(unittest.TestCase):
 def test_wifi_only_provisioning_does_not_add_service_credentials(self):
  c=validate_config({'ssid':'test-network','password':'test-password'})
  for key in ['site_access','participant_credential','bootstrap','enrollment_id']:self.assertEqual(c[key],'')
  with self.assertRaises(ValueError):validate_config({'ssid':'test','password':'test','site_access':'partial'})
  with self.assertRaises(ValueError):validate_config({'ssid':'x'*33,'password':''})
 def test_complete_service_provisioning_remains_supported(self):
  c={'ssid':'test','password':'test','site_access':'test-site','participant_credential':'x'*32,'bootstrap':'test-bootstrap','enrollment_id':'test-enrollment'}
  self.assertEqual(validate_config(c),c)
 def test_embedded_logo(self):
  data=(ROOT/'main/assets/idle_logo.bin').read_bytes()
  self.assertEqual(len(data),48000)
  self.assertEqual(hashlib.sha256(data).hexdigest(),'ff685207d93f71ff55537d42c891cd5a6caa2aba9c8af8090b8e2fcd8c23a421')
  self.assertEqual(data[:2400],bytes(2400));self.assertEqual(data[-2400:],bytes(2400))
  try:from PIL import Image
  except ImportError:return
  from pack import pack
  with Image.open(ROOT/'assets/idle-logo.png') as im:
   self.assertEqual(im.size,(800,480));self.assertEqual(pack(im),data)

 def test_catalog_pins_and_generated_header(self):
  catalog=json.loads((ROOT/'catalog.json').read_text());self.assertEqual(catalog['contract'],CONTRACT);self.assertEqual(catalog['provider'],PROVIDER);self.assertEqual(PROVIDER['provides'][0]['contract_digest'],digest(CONTRACT));self.assertIn(digest(PROFILE),(ROOT/'main/catalog.h').read_text())
 def test_public_geometry(self):
  f=CONTRACT['frame_spec'];self.assertEqual(f['width']*f['height']*f['bits_per_pixel']//8,48000);self.assertEqual(f['tones'],{'0':'white','1':'black'});self.assertEqual(f['tone_levels'],2);self.assertEqual(f['compression'],'none');self.assertEqual(f['stride_bytes'],100)
 def test_raw_exact_size(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'frame';p.write_bytes(bytes(range(256))*187+b'\0'*128);self.assertEqual(len(read_frame(p)),48000);p.write_bytes(b'\0'*47999)
   with self.assertRaises(ValueError):read_frame(p)
 def test_mutation_reply_loss_reuses_exact_identity(self):
  with tempfile.TemporaryDirectory() as d:
   c=Client({'site_access':'not-a-secret-test-value'},str(pathlib.Path(d)/'j.db'));seen=[]
   def lost(path,data):seen.append(data);raise TimeoutError()
   c.request=lost
   with self.assertRaises(TimeoutError):c.call('desired','put',{'state':{}},key='stable-id')
   c.db.close();c=Client({'site_access':'not-a-secret-test-value'},str(pathlib.Path(d)/'j.db'))
   def reply(path,data):seen.append(data);return b'{"protocol_version":"1.0","status":"completed","data":{"id":"ok"}}'
   c.request=reply;self.assertEqual(c.call('desired','put',{'state':{}},key='stable-id'),{'id':'ok'});self.assertEqual(seen[0],seen[1]);self.assertEqual(len(seen),2)
   c.call('desired','put',{'state':{}},key='stable-id');self.assertEqual(len(seen),2)
   with self.assertRaises(RuntimeError):c.call('desired','put',{'state':{'changed':True}},key='stable-id')
 def test_unknown_reply_is_not_cached_as_success(self):
  with tempfile.TemporaryDirectory() as d:
   c=Client({'site_access':'test'},str(pathlib.Path(d)/'j.db'));c.request=lambda *a:b'{"protocol_version":"1.0","mutation_outcome":"unknown"}'
   with self.assertRaises(RuntimeError):c.call('desired','put',{},key='unknown')
   self.assertIsNone(c.db.execute('SELECT response FROM rpc').fetchone()[0])
 def test_packing_black_white_and_pixel_positions(self):
  try:from PIL import Image
  except ImportError:self.skipTest('Optional Pillow not installed')
  from pack import pack
  im=Image.new('RGB',(800,480),'white');im.putpixel((0,0),(0,0,0));im.putpixel((7,0),(0,0,0));im.putpixel((799,479),(0,0,0));data=pack(im);self.assertEqual(data[0],0x81);self.assertEqual(data[-1],1);self.assertEqual(sum(data[1:-1]),0)
  self.assertEqual(pack(Image.new('RGB',(800,480),'black')),b'\xff'*48000)
  self.assertEqual(pack(Image.new('RGBA',(800,480),(0,0,0,0))),b'\0'*48000)
  with self.assertRaises(ValueError):pack(Image.new('RGB',(801,480)))
