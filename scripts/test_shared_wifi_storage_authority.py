#!/usr/bin/env python3
import copy,unittest
from shared_wifi_products import encode
from shared_wifi_storage_authority import census,volume_census
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
class Volumes(unittest.TestCase):
 def test_explicit_absence(self):
  entries={'boot.json':encode({'drivers':[],'installation':{'volume_instance':0}})}
  result=volume_census(entries)
  self.assertTrue(result['selected_volume_provider_absent'])
  self.assertTrue(result['volume_declarations_absent'])
  self.assertEqual(result['bootstrap_declarations'][0]['value'],0)
  self.assertFalse(result['native_bootstrap_registration_verified'])
 def test_provider_and_bindings(self):
  row={'manifest':'v.json','instance_id':17,'bindings':[{'capability':'storage.volume','api':1,'instance_id':0}]}
  entries={'boot.json':encode({'drivers':[row],'platforms':[{'capability':'storage.volume','api':1,'instance_id':23}]}),
   'v.json':encode({'id':'volume','provides':[{'capability':'storage.volume','api':1}]})}
  old=copy.deepcopy(entries);result=volume_census(entries)
  self.assertFalse(result['selected_volume_provider_absent'])
  self.assertEqual(result['selected_volume_providers'][0]['instance_id'],17)
  self.assertEqual(result['selected_volume_providers'][0]['row'],row)
  self.assertEqual(len(result['volume_declarations']),3)
  self.assertEqual(entries,old)
 def test_missing_instance_not_invented(self):
  entries={'boot.json':encode({'drivers':[{'manifest':'v.json'}]}),
   'v.json':encode({'id':'volume','provides':[{'capability':'storage.volume','api':1}]})}
  provider=volume_census(entries)['selected_volume_providers'][0]
  self.assertFalse(provider['instance_id_present']);self.assertIsNone(provider['instance_id'])
if __name__=='__main__':unittest.main()
