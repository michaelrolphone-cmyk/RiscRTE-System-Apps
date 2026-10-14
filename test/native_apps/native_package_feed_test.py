#!/usr/bin/env python3
"""Publisher policy and transactional output tests; ELF admission is separate."""
import importlib.util,json,tempfile,struct,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('feed',root/'scripts/build_native_package_feed.py');feed=importlib.util.module_from_spec(spec);spec.loader.exec_module(feed)
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);data=bytearray(52);data[:6]=b'\x7fELF\x01\x01';struct.pack_into('<H',data,18,94);(p/'app.elf').write_bytes(data)
 manifest={'type':'application','id':'core-app','version':'1.0.0','file_name':'app.elf'};(p/'app.json').write_text(json.dumps(manifest))
 plan={'apps':[{'manifest':'app.json','image':'app.elf'}],'profiles':{'core':['core-app']}}
 def write(value): (p/'plan.json').write_text(json.dumps(value))
 write(plan);receipt=feed.build(p/'plan.json','https://example.com/immutable',p/'good','watch-s3')
 result=json.loads((p/'good/native-packages-v1.json').read_text());assert result['product']=='watch-s3' and result['profiles']==plan['profiles'];assert result['apps'][0]['sha256']==hashlib.sha256(data).hexdigest();assert not receipt['published']
 for index,profiles in enumerate([None,{}, {'core':['missing']},{'core':['core-app','core-app']},{'bad/profile':['core-app']},{'core':[{}]}]):
  bad=dict(plan);bad['profiles']=profiles;write(bad)
  try:feed.build(p/'plan.json','https://example.com/assets',p/f'bad{index}','watch-s3')
  except ValueError:pass
  else:raise AssertionError(profiles)
  assert not (p/f'bad{index}').exists()
 write(plan)
 for product in ['', 'other/product', 'A']:
  try:feed.build(p/'plan.json','https://example.com/assets',p/'bad-product',product)
  except ValueError:pass
  else:raise AssertionError(product)
 assert not (p/'bad-product').exists()
print('Native feed: explicit product and profiles, exact immutable hash, unknown/duplicate/malformed selection rejected before any output PASS')
