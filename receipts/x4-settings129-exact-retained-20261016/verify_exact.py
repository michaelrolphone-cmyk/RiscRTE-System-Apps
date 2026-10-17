#!/usr/bin/env python3
"""Verify complete retained ELF equality and the reconstructed build source binding."""
import argparse, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--retained',required=True,type=Path)
p.add_argument('--build',required=True,type=Path)
p.add_argument('--source',required=True,type=Path)
a=p.parse_args()
want='1c4914ed12749cb076019dd255c40939d73a6be10f1eda069c4e5d68b48148b7'
retained=a.retained.read_bytes();rebuilt=(a.build/'settings-discard.elf').read_bytes()
assert len(retained)==153132 and hashlib.sha256(retained).hexdigest()==want
assert retained==rebuilt, 'complete ELF differs'
r=json.loads((a.build/'settings-build-record.json').read_text())
for name,digest in r['source_sha256'].items():
 assert hashlib.sha256((a.source/name).read_bytes()).hexdigest()==digest, name
sdk=json.loads((a.source/'test/native_apps/retained_rtc_sdk_hashes.json').read_text())
for name,digest in sdk.items():
 assert hashlib.sha256((a.build/'native-time-sdk'/name).read_bytes()).hexdigest()==digest, name
print(json.dumps({'whole_elf_identical':True,'size_bytes':len(retained),'sha256':want,'production_sources_verified':len(r['source_sha256']),'effective_sdk_files_verified':len(sdk)},indent=2))
