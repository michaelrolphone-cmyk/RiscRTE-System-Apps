#!/usr/bin/env python3
"""Pure receipt-binding regressions; no product admission claimed."""
import ast,hashlib,pathlib,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
source=(ROOT/'scripts/test_home_product_admission.py').read_text()
tree=ast.parse(source)
namespace={'pathlib':pathlib,'hashlib':hashlib,'json':__import__('json'),
           'sha':lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()}
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('tree_pins','canonical_digest')],type_ignores=[]),'<actual-binding-functions>','exec'),namespace)
pins=namespace['tree_pins'];digest=namespace['canonical_digest']
class Binding(unittest.TestCase):
 def test_order_independent(self):
  self.assertEqual(digest({'b':'2','a':'1'}),digest({'a':'1','b':'2'}))
 def test_actual_bytes_and_path_bound(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp)/'after';root.mkdir();(root/'home.elf').write_bytes(b'one')
   first=digest(pins(root));(root/'home.elf').write_bytes(b'two')
   self.assertNotEqual(first,digest(pins(root)))
   second=digest(pins(root));(root/'home.elf').rename(root/'other.elf')
   self.assertNotEqual(second,digest(pins(root)))
 def test_missing_and_empty_internal_refused(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp)/'after'
   with self.assertRaises(ValueError):pins(root)
   root.mkdir()
   with self.assertRaises(ValueError):pins(root)
 def test_explicit_empty_external_inventory(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp)/'after-sd';self.assertEqual(pins(root),{})
   root.mkdir();self.assertEqual(pins(root),{})
 def test_symlink_refused(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp)/'after';root.mkdir();outside=pathlib.Path(tmp)/'real';outside.write_bytes(b'data')
   (root/'link').symlink_to(outside)
   with self.assertRaises(ValueError):pins(root)
if __name__=='__main__':unittest.main()
