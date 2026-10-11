#!/usr/bin/env python3
"""Create an explicit build-owned V2 migration policy; never edits a store."""
import argparse,hashlib,json,re
from pathlib import Path

def generate(source,target,output,shared_kv):
    source,target,output=map(Path,(source,target,output))
    before=json.loads((source/'boot.json').read_text());after=json.loads((target/'boot.json').read_text())
    identities=[json.loads((p/'cohort.json').read_text()) for p in (source,target)]
    def coordinate(c):
        return '{'+','.join(json.dumps(c[k]) for k in ('product','version','source_repo','source_revision','runtime_version'))+'}'
    def policies(root,boot):
        return {json.loads((root/p['manifest']).read_text())['id']:p for p in boot['app_capabilities']}
    old,new=policies(source,before),policies(target,after)
    records=[];declarations=[]
    for i,spec in enumerate(shared_kv):
        consumer,api,ns=spec.split(':');api,ns=int(api),int(ns)
        assert consumer in new and api in (1,2) and 0<ns<2**31
        grant={'capability':'storage.key-value','api':api,'instance_id':ns}
        assert grant in new[consumer]['grants'] and grant not in old.get(consumer,{}).get('grants',[])
        owners=sorted(k for k,v in old.items() if grant in v['grants']);assert owners
        declarations.append('static constexpr const char* owners'+str(i)+'[]={'+','.join(map(json.dumps,owners))+'};')
        records.append('{'+','.join((json.dumps(consumer),str(api),str(ns),'owners'+str(i),str(len(owners))))+'}')
    if records:declarations.append('static constexpr NativeSharedKeyValueV2 shared[]={'+','.join(records)+'};')
    snapshots=[json.dumps(b['app_capabilities'],sort_keys=True,separators=(',',':')) for b in (before,after)]
    assert all(len(s.encode())<65536 for s in snapshots)
    key=hashlib.sha256((''.join(snapshots)+json.dumps(identities,sort_keys=True)).encode()).hexdigest()
    code='#pragma once\n#include "runtime/update/NativeAppDataExportPolicyV2.h"\nnamespace RiscUpdate {\n'+'\n'.join(declarations)+'\n'
    code+='inline constexpr NativeAppDataExportPolicyV2 selectedNativeAppDataExportPolicyV2(){return {2,'+json.dumps('export-'+key[:32])+','+','.join(map(coordinate,identities))+','+','.join(json.dumps(s) for s in snapshots)+(','+'shared,'+str(len(records)) if records else ',nullptr,0')+'};}\n}\n'
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(code)
    receipt={'schema':2,'source':identities[0],'target':identities[1],'shared_kv':shared_kv,'header_sha256':hashlib.sha256(code.encode()).hexdigest(),'boot_sha256':[hashlib.sha256((p/'boot.json').read_bytes()).hexdigest() for p in (source,target)]}
    output.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n');return receipt
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source','target','output'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--shared-kv',action='append',default=[],help='Explicit consumer:API:namespace transition; repeat as needed')
    a=p.parse_args();print(json.dumps(generate(a.source,a.target,a.output,a.shared_kv),indent=2))
