#!/usr/bin/env python3
import copy,unittest
from shared_wifi_products import encode
from shared_wifi_storage_authority import census
class Authority(unittest.TestCase):
 def test_all_namespaces_and_bound_keys(self):
  grants=[{'capability':cap,'api':1,'instance_id':ns} for cap,ns in [('storage.key-value',1),('storage.key-value.atomic',6),('storage.key-value.availability',6),('storage.preferences',17)]]
  keys=[{'namespace':1,'key':'read','access':'read'},{'namespace':17,'key':'write','access':'read-write'}]
  boot={'app_capabilities':[{'manifest':'a.json','grants':grants}],'drivers':[{'manifest':'p.json','key_value':keys}]}
  entries={'boot.json':encode(boot),'a.json':encode({'id':'any-app'}),'p.json':encode({'id':'any-provider'})};old=copy.deepcopy(entries)
  r=census(entries)
  self.assertEqual(set(r['namespaces']),{'1','6','17'})
  self.assertEqual([x['potential_writer'] for x in r['namespaces']['1']],[True,False])
  self.assertEqual([x['potential_writer'] for x in r['namespaces']['6']],[True,False])
  self.assertFalse(r['migration_approved']);self.assertFalse(r['native_writer_census_complete']);self.assertEqual(entries,old)
  self.assertEqual(r['namespaces']['17'][1]['bound_key'],keys[1])
 def test_ambiguous_namespace_and_access(self):
  boot={'app_capabilities':[{'manifest':'a.json','grants':[{'capability':'storage.key-value','api':1}]}]}
  entries={'boot.json':encode(boot),'a.json':encode({'id':'a'})}
  self.assertTrue(census(entries)['unresolved_instance_zero'])
  boot['drivers']=[{'manifest':'a.json','key_value':[{'namespace':1,'key':'a','access':'unknown'}]}];entries['boot.json']=encode(boot)
  with self.assertRaises(ValueError):census(entries)
if __name__=='__main__':unittest.main()
