import hashlib,json,re,struct,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class TimecardPreparation(unittest.TestCase):
 def test_prepared_glyph_is_exact_and_adds_no_catalog_entry(self):
  root=ROOT/'lib/PortableApps';data=(root/'quick_fonts/FAClassicSolid_18.cpfont').read_bytes()
  self.assertEqual(hashlib.sha256(data).hexdigest(),'7b2f820afb525f62c280292e63b01ef302e8392c1fe6fc707719588c41c8204c')
  _,ni,ng,_,_,_,kl,kr,lc,rc,lig,off=struct.unpack_from('<B3xIIBhhHHBBBI4x',data,32)
  intervals=[struct.unpack_from('<III',data,off+12*i) for i in range(ni)]
  index=next(base+0xf274-lo for lo,hi,base in intervals if lo<=0xf274<=hi)
  goff=off+ni*12;boff=goff+ng*16+(kl+kr)*3+lc*rc+lig*8
  w,h,_,_,_,length,offset=struct.unpack_from('<BBHhhH2xI',data,goff+16*index)
  source=(root/'fonts/icons.inc').read_text()
  encoded=re.search(r'rpi_solid_f274\[\] = \{([^}]+)\}',source).group(1)
  self.assertEqual(bytes(map(int,encoded.split(','))),data[boff+offset:boff+offset+length])
  self.assertIn('{"solid:f274",%d,%d,rpi_solid_f274}'%(w,h),source)
  self.assertNotIn('timecard',json.loads((root/'catalog-icons.json').read_text())['apps'])
  self.assertEqual(json.loads((root/'additional-icons.json').read_text()),{'solid:f274':'calendar-check','solid:f245':'arrow-pointer','solid:f015':'house'})
 def test_declared_capacity_and_version_are_explicit(self):
  source=(ROOT/'lib/PortableApps/src/adapter.c').read_text()
  self.assertEqual(source.count('portable_catalog_count <= PORTABLE_CATALOG_LIMIT'),2)
  header=(ROOT/'lib/PortableApps/include/PortableApps.h').read_text()
  self.assertIn('#define PORTABLE_CATALOG_LIMIT 17',header)
  self.assertIn('PORTABLE_CATALOG_LIMIT < 17 || PORTABLE_CATALOG_LIMIT > 21',header)
  self.assertEqual(json.loads((ROOT/'Apps/springboard.json').read_text())['version'],'1.7.12')

if __name__=='__main__':unittest.main()
