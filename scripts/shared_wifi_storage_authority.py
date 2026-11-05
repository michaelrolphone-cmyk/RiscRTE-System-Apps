"""Read-only storage-authority census. Grants are potential writers, not usage proof."""
import json
from shared_wifi_products import check,sha,safe
CAPABILITIES={'storage.key-value','storage.key-value.atomic','storage.preferences','storage.key-value.availability'}
def census(entries):
    boot=json.loads(entries['boot.json']);result={}
    def add(ns,item):
        check(type(ns) is int and ns>=0,'Invalid storage namespace')
        result.setdefault(str(ns),[]).append(item)
    for kind,key in (('application','app_capabilities'),('provider','drivers')):
        for row in boot.get(key,[]):
            path=safe(row['manifest']);raw=entries[path];manifest=json.loads(raw)
            identity={'kind':kind,'id':manifest['id'],'manifest':path,'manifest_sha256':sha(raw)}
            for grant in row.get('grants',[]) if kind=='application' else row.get('bindings',[]):
                if grant.get('capability') in CAPABILITIES:
                    ns=grant.get('instance_id',0)
                    add(ns,{**identity,'authority':grant,'potential_writer':grant['capability']!='storage.key-value.availability','usage_verified':False})
            if kind=='provider':
                for binding in row.get('key_value',[]):
                    access=binding.get('access')
                    check(access in ('read','write','read-write'),'Unknown bound storage access')
                    add(binding['namespace'],{**identity,'bound_key':binding,'potential_writer':access!='read','usage_verified':False})
    return {'schema':1,'boot_sha256':sha(entries['boot.json']),'namespaces':dict(sorted(result.items(),key=lambda x:int(x[0]))),
            'scope':'declared authority only; no device data or native writer registration inspected',
            'migration_approved':False,'native_writer_census_complete':False,
            'unresolved_instance_zero':bool(result.get('0'))}
