#!/usr/bin/env python3
"""Publication corruption guards. No external publication is invoked."""
import argparse,json
from pathlib import Path
from publish_touchpad_shared import verify
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--source',required=True);a=p.parse_args();root=a.build.resolve()
verify(root,a.source)
assembly=json.loads((root/'assembly/assembly.json').read_text())
unchanged=next(n for n in sorted((root/'assembly/watch/after').rglob('*')) if n.is_file() and n.relative_to(root/'assembly/watch/after').as_posix() not in assembly['products']['watch']['changed_internal'])
def reject(name,path,replace):
 original=path.read_bytes()
 try:
  path.write_bytes(replace(original))
  try:verify(root,a.source)
  except (ValueError,AssertionError,KeyError):print(name+' rejected',flush=True)
  else:raise RuntimeError('Publisher accepted corrupted '+name)
 finally:path.write_bytes(original)
reject('source',root/'source/System.head',lambda _:b'0'*40+b'\n')
reject('application',root/'application/packages/ble_touchpad/ble_touchpad.elf',lambda b:bytes([b[0]^1])+b[1:])
reject('store',root/'assembly/watch/store.bin',lambda b:bytes([b[0]^1])+b[1:])
reject('unrelated bytes',unchanged,lambda b:b+b'x')
reject('external application',root/'assembly/x4/after-sd'/assembly['products']['x4']['application_route']['path'],lambda b:bytes([b[0]^1])+b[1:])
def wrong_admission(b):
 q=json.loads(b);q['product_binding']['source_commit']='0'*40;return json.dumps(q).encode()
reject('admission source',root/'admission/normal/receipt.json',wrong_admission)
def missing_confirm(b):
 q=json.loads(b);q['passed']=[x for x in q['passed'] if x['case']!='confirm_visible'];return json.dumps(q).encode()
reject('visible confirmation evidence',root/'pointer-normal/results.json',missing_confirm)
reject('source archive',root/'source/System.tar.gz',lambda b:bytes([b[0]^1])+b[1:])
verify(root,a.source)
print('Touchpad publication boundary: 8 corruptions rejected; original closure accepted',flush=True)
