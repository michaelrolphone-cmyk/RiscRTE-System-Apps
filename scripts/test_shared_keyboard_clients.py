#!/usr/bin/env python3
"""Replay selected production Home/Points/BLE text clients using a Runtime fixture.
The external capacity fixture has an obsolete hard-coded portrait90 build flag;
replace that one test policy with the installed portrait270 contract and record
both exact script inputs. No production sources or Runtime files are modified.
"""
import argparse, hashlib, json, sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a,extra=p.parse_known_args()
runner=a.runtime.resolve()/'test/run_shared_keyboard_resident.py'
source=runner.read_text();before="'-DSCENE_DISPLAY_ROTATION=90'";after="'-DSCENE_DISPLAY_ROTATION=270'"
assert source.count(before)==1,'Reinspect changed external fixture before execution'
patched=source.replace(before,after)
a.output.mkdir(parents=True,exist_ok=True)
receipt={'external_runner':str(runner),'external_sha256':hashlib.sha256(source.encode()).hexdigest(),
         'executed_sha256':hashlib.sha256(patched.encode()).hexdigest(),
         'replacement':{'before':before,'after':after},'wrapper_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
(a.output/'fixture-policy-override.json').write_text(json.dumps(receipt,indent=2)+'\n')
sys.argv=[str(runner),'--output',str(a.output),*extra]
exec(compile(patched,str(runner),'exec'),{'__file__':str(runner),'__name__':'__main__'})
