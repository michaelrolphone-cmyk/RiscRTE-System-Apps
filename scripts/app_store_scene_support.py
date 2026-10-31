"""One flattened SDK, with Runtime's canonical contracts taking precedence."""
from pathlib import Path
import json, shutil, hashlib

def stage(runtime, system, output, utilities=None):
    runtime, system, output = map(Path, (runtime, system, output))
    include = output / 'include'
    include.mkdir(parents=True, exist_ok=True)
    sources = {}
    for directory in (system/'lib/PortableApps/include', system/'sdk/app', runtime/'sdk/app', runtime/'sdk/driver', runtime/'sdk/hardware'):
        for p in directory.glob('*.h'):
            sources[p.name] = p
    if utilities is not None:
        utilities=Path(utilities)
        origin=json.loads((system/'test/app_store_background/source-origin.json').read_text())
        for relative, expected in origin['blobs'].items():
            p=utilities/relative; data=p.read_bytes()
            if hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()!=expected:
                raise ValueError('Published Utilities input changed: '+relative)
            sources[p.name]=p
    for name, p in sources.items():
        shutil.copyfile(p, include/name)
    (output/'sdk-origins.json').write_text(json.dumps({k:str(v.resolve()) for k,v in sources.items()},indent=2)+'\n')
    return include
