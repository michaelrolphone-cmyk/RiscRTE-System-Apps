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

def volume_census(entries):
    """Exact decoded declarations, not proof of live media or native registration."""
    boot=json.loads(entries['boot.json']);providers=[];mentions=[];facts=[]
    def walk(value,path,source):
        if isinstance(value,dict):
            if value.get('capability')=='storage.volume':
                mentions.append({'source':source,'path':path,'declaration':value})
            for key,item in value.items():
                if key in ('volume_instance','volume_instance_id') or key.startswith('native_settings'):
                    facts.append({'source':source,'path':path+[key],'value':item})
                walk(item,path+[key],source)
        elif isinstance(value,list):
            for index,item in enumerate(value):walk(item,path+[index],source)
    walk(boot,[],'boot.json')
    for row in boot.get('drivers',[]):
        path=safe(row['manifest']);raw=entries[path];manifest=json.loads(raw)
        declared=[p for p in manifest.get('provides',[]) if p.get('capability')=='storage.volume']
        if declared:
            providers.append({'id':manifest['id'],'manifest':path,'manifest_sha256':sha(raw),
                'instance_id_present':'instance_id' in row,'instance_id':row.get('instance_id'),
                'row':row,'provides':declared})
        walk(manifest,[],path)
    return {'schema':1,'boot_sha256':sha(entries['boot.json']),
        'selected_volume_providers':providers,'volume_declarations':mentions,
        'bootstrap_declarations':facts,'installation':boot.get('installation'),
        'platform':boot.get('platform'),'platforms':boot.get('platforms'),
        'selected_volume_provider_absent':not providers,
        'volume_declarations_absent':not mentions,
        'native_bootstrap_registration_verified':False,
        'scope':'Exact selected boot/manifests only; absence here does not establish firmware support, live media or a valid native bootstrap volume'}
