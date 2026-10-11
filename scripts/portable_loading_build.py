"""Use the deployment's existing catalog identity in the one resident host."""
import hashlib
import json
from build_portable_springboard import catalog_source


def source(root, path, launcher):
    text, record = catalog_source(path, 40)
    apps = list(record['apps'])
    if not any(app['file_name'] == launcher for app in apps):
        manifest = json.loads((root/'Apps/springboard.json').read_text())
        if launcher != manifest['file_name']:
            raise ValueError('Loading catalog must identify the selected launcher')
        apps.append({key: manifest[key] for key in ('display_name', 'file_name', 'icon')})
    rows = ['{'+','.join('.'+key+'='+json.dumps(app[key]) for key in
            ('display_name', 'file_name', 'icon'))+',.compatible=true}' for app in apps]
    text = '#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={' + ','.join(rows) + '};\n'
    text += 'const unsigned portable_catalog_count='+str(len(rows))+';\n'
    return text, {'catalog': record, 'apps': apps, 'owner': 'resident-host',
                  'indicator': 'indeterminate-loading-dots', 'intent': 'LOW_LATENCY',
                  'lifetime': 'completed-before-load; retained-until-child-frame',
                  'fallback': 'filename and existing Apps glyph',
                  'source_sha256': {name: hashlib.sha256((root/name).read_bytes()).hexdigest()
                                    for name in ('scripts/portable_loading_build.py',
                                                 'lib/PortableApps/src/resident_loading.inc')}}
