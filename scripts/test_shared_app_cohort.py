#!/usr/bin/env python3
"""Three-way inventory/authority regression; no device-specific composition."""
import copy, json, unittest
from compose_shared_app_cohort import apply,encode,safe
class Merge(unittest.TestCase):
 def base(self):
  return {'boot.json':encode({'drivers':[{'manifest':'scene/manifest.json','bindings':[]}],
   'app_capabilities':[{'manifest':'files.json','grants':[]},{'manifest':'viewer.json','grants':[]}],
   'namespace_map':{'private':61}}),'files.elf':b'old-files','viewer.elf':b'old-viewer','private/data':b'saved'}
 def edit(self,base,role):
  out=dict(base);out[role+'.elf']=role.encode()
  b=json.loads(out['boot.json'])
  next(r for r in b['app_capabilities'] if r['manifest']==role+'.json')['grants']=[{'capability':'ui.'+role,'api':1,'instance_id':0}]
  b['drivers'].append({'manifest':role+'/manifest.json','bindings':[]});out['boot.json']=encode(b)
  return out
 def test_disjoint_commute(self):
  b=self.base();a=self.edit(b,'files');v=self.edit(b,'viewer')
  x=apply(b,apply(b,b,a),v);y=apply(b,apply(b,b,v),a)
  self.assertEqual({k:v for k,v in x.items() if k!='boot.json'},{k:v for k,v in y.items() if k!='boot.json'})
  xb=json.loads(x['boot.json']);yb=json.loads(y['boot.json'])
  self.assertEqual(xb['app_capabilities'],yb['app_capabilities'])
  self.assertEqual(sorted(xb['drivers'],key=lambda r:r['manifest']),sorted(yb['drivers'],key=lambda r:r['manifest']))
  self.assertEqual(x['private/data'],b'saved');self.assertEqual(xb['namespace_map'],{'private':61})
 def test_same_delta_idempotent(self):
  b=self.base();a=self.edit(b,'files');self.assertEqual(apply(b,a,a),a)
 def test_conflicting_bytes_refused(self):
  b=self.base()
  with self.assertRaises(ValueError):apply(b,{**b,'files.elf':b'first'},{**b,'files.elf':b'second'})
 def test_conflicting_grants_refused(self):
  b=self.base();a=self.edit(b,'files');v=self.edit(b,'files')
  j=json.loads(v['boot.json']);j['app_capabilities'][0]['grants'][0]['instance_id']=9;v['boot.json']=encode(j)
  with self.assertRaises(ValueError):apply(b,a,v)
 def test_deletion_preserved(self):
  b=self.base();a=dict(b);del a['files.elf'];self.assertNotIn('files.elf',apply(b,b,a))
 def test_unrelated_boot_conflict(self):
  b=self.base();a=dict(b);v=dict(b)
  for d,n in ((a,62),(v,63)):
   j=json.loads(d['boot.json']);j['namespace_map']['private']=n;d['boot.json']=encode(j)
  with self.assertRaises(ValueError):apply(b,a,v)
 def test_duplicate_rows_rejected(self):
  b=self.base();a=dict(b);j=json.loads(a['boot.json']);j['drivers']*=2;a['boot.json']=encode(j)
  with self.assertRaises(ValueError):apply(b,b,a)
 def test_same_driver_multiple_hardware_instances(self):
  b=self.base();j=json.loads(b['boot.json'])
  j['drivers']=[{'manifest':'bus/manifest.json','instance_id':1,'bindings':[]},{'manifest':'bus/manifest.json','instance_id':2,'bindings':[]}]
  b['boot.json']=encode(j);a=dict(b);v=dict(b)
  ja=copy.deepcopy(j);ja['drivers'][0]['bindings']=[{'capability':'one','instance_id':1,'api':1}];a['boot.json']=encode(ja)
  jv=copy.deepcopy(j);jv['drivers'][1]['bindings']=[{'capability':'two','instance_id':2,'api':1}];v['boot.json']=encode(jv)
  out=json.loads(apply(b,apply(b,b,a),v)['boot.json'])
  self.assertEqual(out['drivers'],[ja['drivers'][0],jv['drivers'][1]])
 def test_unsafe_paths(self):
  for n in ('../x','/absolute','a/../../b','a\\b',''):
   with self.assertRaises(ValueError):safe(n)
if __name__=='__main__':unittest.main()
