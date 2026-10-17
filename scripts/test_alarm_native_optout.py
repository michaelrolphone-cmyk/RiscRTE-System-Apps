#!/usr/bin/env python3
"""Compare selector-off production tokens with the reviewed pre-alarm commit."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline',default='fbf74c263721a5e48c3259e6551f1c195af58464');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);base=out/'baseline';base.mkdir(exist_ok=True)
paths=['lib/PortableApps/src/adapter.c','lib/PortableApps/src/alarm.inc']
for name in paths:
    (base/Path(name).name).write_bytes(subprocess.check_output(['git','show',a.baseline+':'+name],cwd=ROOT))
rows=[]
for profile,defines in [('plain',[]),('nova',['PORTABLE_NOVA_UI']),
    ('height',['PORTABLE_NOVA_UI','PORTABLE_WATCH_NATIVE_HEIGHT']),
    ('native',['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS']),
    ('paper',['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_PAPER_PREFERENCES'])]:
    for quick in (False,True):
        for snapshot in (False,True):
            flags=['PORTABLE_ALARM_CLIENT',*defines]
            if quick:flags+=['PORTABLE_QUICK_ACTIONS']
            if snapshot:flags+=['PORTABLE_RASTER_SNAPSHOT']
            tokens=[]
            for source in (base/'adapter.c',ROOT/'lib/PortableApps/src/adapter.c'):
                cmd=[os.environ.get('CC','cc'),'-E','-P',*('-D'+f for f in flags),
                    '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
                    '-I'+str(ROOT/'lib/PortableApps/src'),str(source)]
                result=subprocess.run(cmd,text=True,capture_output=True)
                (out/f'{profile}-quick{int(quick)}-snapshot{int(snapshot)}-{source.parent.name}.log').write_text(result.stderr)
                result.check_returncode()
                data=result.stdout.replace(str(base),'$ROOT').replace(str(ROOT),'$ROOT')
                tokens.append('\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''',data)))
            assert tokens[0]==tokens[1],(profile,quick,snapshot)
            rows.append({'profile':profile,'quick':quick,'snapshot':snapshot,'identical':True,
                'token_sha256':hashlib.sha256(tokens[0].encode()).hexdigest()})
(out/'results.json').write_text(json.dumps({'baseline':a.baseline,'rows':rows,
    'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths}},indent=2)+'\n')
print(f'{len(rows)} selector-off adapter compositions have identical preprocessed tokens')
