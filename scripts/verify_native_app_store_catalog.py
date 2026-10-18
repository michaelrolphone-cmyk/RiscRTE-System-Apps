#!/usr/bin/env python3
"""Verify an additive catalog binding proposal without mutating a catalog.

Before/after inputs are catalog exports of {"entries": [...]} with full entry
objects preserved. The sole addition must equal the release's binding record.
The caller owns adapting its external catalog's exact schema to this contract.
"""
import argparse
import json
from pathlib import Path
from native_app_store_evidence import read, require, sha


def canonical(value):
    return json.dumps(value,sort_keys=True,separators=(',',':'))


def verify(before,after,binding,audit):
    prior=read(before); proposed=read(after); expected=read(binding); public=read(audit)
    require(set(prior)=={'entries'} and set(proposed)=={'entries'}, 'expected complete entry exports')
    require(public['status']=='PASS' and public['all_public_assets_byte_verified'] is True and
            public['tag']==expected['release_tag'] and public['source_commit']==expected['source_commit'],
            'exact public byte audit missing')
    require(public['public_asset_sha256']['app_store.elf']==expected['image']['sha256'] and
            public['public_asset_sha256']['app_store.json']==expected['manifest_sha256'] and
            public['public_asset_sha256']['catalog-binding.json']==sha(binding), 'binding differs from public bytes')
    old=[canonical(row) for row in prior['entries']]; new=[canonical(row) for row in proposed['entries']]
    require(len(set(old))==len(old) and len(set(new))==len(new), 'duplicate catalog entry')
    require(canonical(expected) not in old and set(new)-set(old)=={canonical(expected)} and
            set(old)<=set(new) and len(new)==len(old)+1, 'catalog must preserve every old entry and add exactly the verified binding')
    return {'status':'PASS','added_application':expected['application'],'version':expected['version'],
            'preserved_entries':len(old),'before_sha256':sha(before),'after_sha256':sha(after),
            'binding_sha256':sha(binding),'public_audit_sha256':sha(audit),'catalog_mutated':False}


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('before','after','binding','audit'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();print(json.dumps(verify(a.before,a.after,a.binding,a.audit),indent=2))
