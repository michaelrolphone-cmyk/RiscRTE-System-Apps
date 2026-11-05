#!/usr/bin/env python3
"""Preserve all 20 selector-off compositions against the reviewed pre-alarm source.

The old baseline contains the independently reproduced paper/native-height bug.
Apply one fixed source-matched guard to that frozen baseline before comparison;
all other preprocessed tokens must remain identical. The unmodified baseline
failures and real paper/RGB behavior are qualified separately.
"""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline-dir',type=Path,help='Exact frozen baseline files for an offline checkout')
p.add_argument('--baseline',default='fbf74c263721a5e48c3259e6551f1c195af58464');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);base=out/'baseline';base.mkdir(exist_ok=True)
paths=['lib/PortableApps/src/adapter.c','lib/PortableApps/src/alarm.inc']
for name in paths:
    data=(a.baseline_dir/Path(name).name).read_bytes() if a.baseline_dir else subprocess.check_output(['git','show',a.baseline+':'+name],cwd=ROOT)
    expected_blob={'adapter.c':'9f778a87b3cb520d02fc4aac647f4879f39ef780','alarm.inc':'055522897489f8cae1958763176fb43dfddd7b32'}[Path(name).name]
    assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==expected_blob, ('Unexpected frozen baseline',name)
    (base/Path(name).name).write_bytes(data)
# This literal expectation is independent of current candidate source.
old_guard = "if(info.width){sample.x=(uint32_t)sample.x*240u/info.width;sample.y=(uint32_t)sample.y*240u/info.width;}"
new_guard = "if(!pp_enabled() && info.width){sample.x=(uint32_t)sample.x*240u/info.width;sample.y=(uint32_t)sample.y*240u/info.width;}"
baseline_alarm=base/'alarm.inc'; original=baseline_alarm.read_bytes()
assert hashlib.sha1(b'blob '+str(len(original)).encode()+b'\0'+original).hexdigest()=='055522897489f8cae1958763176fb43dfddd7b32', 'Unexpected pre-alarm source'
original_text=original.decode(); assert original_text.count(old_guard)==1
baseline_alarm.write_text(original_text.replace(old_guard,new_guard))
normalization={'reason':'Independently reproduced paper native-height coordinate correction',
 'original_git_blob':'055522897489f8cae1958763176fb43dfddd7b32',
 'original_sha256':hashlib.sha256(original).hexdigest(),
 'fixed_guard_patch_sha256':hashlib.sha256((old_guard+'\n'+new_guard+'\n').encode()).hexdigest(),
 'normalized_sha256':hashlib.sha256(baseline_alarm.read_bytes()).hexdigest()}
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
(out/'results.json').write_text(json.dumps({'baseline':a.baseline,'baseline_normalization':normalization,'rows':rows,
    'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths}},indent=2)+'\n')
print(f'{len(rows)} selector-off adapter compositions have identical tokens after the fixed, source-matched paper correction')
