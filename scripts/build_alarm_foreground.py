#!/usr/bin/env python3
"""Build the shared foreground alarm provider; no installation or parity claim."""
import argparse, hashlib, json, shutil, subprocess, tempfile
from pathlib import Path
from test_alarm_foreground import stage
from scene_build import build, compiler_path
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,output,tagged=False):
    if output.exists():raise FileExistsError(output)
    name='tagged-manifest.json' if tagged else 'manifest.json'
    manifest=json.loads((ROOT/'Services/alarm_foreground'/name).read_text())
    with tempfile.TemporaryDirectory(prefix='alarm-foreground-sdk-') as tmp:
        inc=stage(runtime,Path(tmp))
        row=build(compiler_path(),inc,output,manifest,[ROOT/'Services/alarm_foreground/host.c'],
                  ['-Wno-misleading-indentation','-Wframe-larger-than=1536'],exports={'t5_driver_get'})
    shutil.copyfile(ROOT/'Services/alarm_foreground'/name,output/'manifest.json')
    shutil.copyfile(ROOT/'Services/alarm_foreground/UTILITIES_LICENSE',output/'UTILITIES_LICENSE')
    shutil.copyfile(ROOT/'Services/alarm_foreground/provenance.json',output/'provenance.json')
    row['source_pins']={name:subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
                        for name,path in (('system',ROOT),('runtime',runtime))}
    row['manifest_sha256']=hashlib.sha256((output/'manifest.json').read_bytes()).hexdigest()
    row['working_tree_dirty']=subprocess.run(['git','-C',str(ROOT),'diff','--quiet','HEAD','--']).returncode!=0
    row['qualification']='candidate build only; installed scheduler and product receipts required'
    (output/'build.json').write_text(json.dumps(row,indent=2,sort_keys=True)+'\n')
    return row
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--tagged',action='store_true');a=p.parse_args()
    print(json.dumps(run(a.runtime.resolve(),a.output.resolve(),a.tagged),indent=2))
