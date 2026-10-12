#!/usr/bin/env python3
"""Production Quick controller/session/radio atomic workflow fault matrix."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES='queued-controls queue-overflow raw-brightness raw-tone raw-coalesced raw-stale raw-cancel repeated brightness off off-already restore-stale tone dnd contexts radio same-value preview restore origins automatic copy busy stale display-stale cancel cancel-race cancel-error small-group wrong-namespace read-io missing submit-io uncertain capacity conflict dirty-submit collect-io bad-receipt release-failed close-pending'.split()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 receipt=json.loads((a.target/'build-evidence.json').read_text());inc=a.output/'include';shutil.copytree(receipt['paper_transition']['compiled_include_directory'],inc,dirs_exist_ok=True)
 for header in (ROOT/'lib/PortableApps/include').glob('*.h'):
  if not header.name.startswith(('Risc','AlarmService')):shutil.copyfile(header,inc/header.name)
 flags=['-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DPORTABLE_FRONTLIGHT_TONE','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_RESIDENT_SHELL_HOST']
 sources=[ROOT/'test/native_apps/quick_atomic_workflow_test.c',*[ROOT/'lib/PortableApps/src'/name for name in ['quick_session.c','quick_radios.c','quick_actions.c']]]
 binary=a.output/'quick-atomic';extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*extra,*flags,'-I'+str(inc),*map(str,sources),'-o',str(binary)]
 subprocess.run(cmd,check=True);results=[]
 for case in CASES:
  r=subprocess.run([binary,case],check=True,timeout=10,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));print(r.stdout.strip());results.append({'case':case,'result':r.stdout.strip()})
 files=[*sources,ROOT/'lib/PortableApps/src/quick_atomic.inc',ROOT/'lib/PortableApps/include/PortablePreferenceAtomic.h',Path(__file__).resolve()]
 (a.output/'receipt.json').write_text(json.dumps({'suite':'actual Quick controller staging and session/radio paths','runtime_provider_double':True,'hardware_tested':False,'sanitized':a.sanitize,'cases':results,'compile_command':cmd,'sha256':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
if __name__=='__main__':main()
