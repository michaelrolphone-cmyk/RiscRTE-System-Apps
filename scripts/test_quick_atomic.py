#!/usr/bin/env python3
"""Production Quick controller/session/radio atomic workflow fault matrix."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CASES='rgb-mute-io rgb-mute-cancel rgb-brightness rgb-volume rgb-mute rgb-mute-already rgb-mute-restore-stale rgb-volume-stale queued-controls queue-overflow raw-brightness raw-tone raw-coalesced raw-stale raw-cancel repeated brightness off off-already restore-stale tone dnd contexts radio same-value preview restore origins automatic copy busy stale display-stale cancel cancel-race cancel-error small-group wrong-namespace read-io missing submit-io uncertain capacity conflict dirty-submit collect-io bad-receipt release-failed close-pending'.split()
def main():
 p=argparse.ArgumentParser(description=__doc__);inputs=p.add_mutually_exclusive_group(required=True);inputs.add_argument('--target',type=Path);inputs.add_argument('--sdk',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 source_sdk=a.sdk if a.sdk else Path(json.loads((a.target/'build-evidence.json').read_text())['paper_transition']['compiled_include_directory']);inc=a.output/'include';shutil.copytree(source_sdk,inc,dirs_exist_ok=True)
 for header in (ROOT/'lib/PortableApps/include').glob('*.h'):
  if not header.name.startswith(('Risc','AlarmService')):shutil.copyfile(header,inc/header.name)
 flags=['-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_FRONTLIGHT_TONE','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_RESIDENT_SHELL_HOST']
 sources=[ROOT/'test/native_apps/quick_atomic_workflow_test.c',*[ROOT/'lib/PortableApps/src'/name for name in ['quick_session.c','quick_radios.c','quick_actions.c']]]
 binary=a.output/'quick-atomic';extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*extra,*flags,'-I'+str(inc),*map(str,sources),'-o',str(binary)]
 subprocess.run(cmd,check=True);results=[]
 # Compile the full RGB adapter without paper, tone, or resident-Home flags.
 shutil.copytree(ROOT/'lib/PortableApps/time',a.output/'time',dirs_exist_ok=True)
 rgb_cmd=['cc','-std=c11','-O0','-Wall','-Wextra','-Werror','-Wno-unused-function',*extra,*['-D'+x for x in ('PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_ALARM_CLIENT','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_CHECKED_STORAGE_RUNTIME','PORTABLE_SETTINGS_APP')],'-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),'-c',str(ROOT/'lib/PortableApps/src/adapter.c'),'-o',str(a.output/'rgb-adapter.o')]
 subprocess.run(rgb_cmd,check=True)
 for case in CASES:
  r=subprocess.run([binary,case],check=True,timeout=10,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));print(r.stdout.strip());results.append({'case':case,'result':r.stdout.strip()})
 files=[*sources,ROOT/'lib/PortableApps/src/adapter.c',ROOT/'lib/PortableApps/include/PortableQuickSession.h',ROOT/'lib/PortableApps/src/quick_atomic.inc',ROOT/'lib/PortableApps/include/PortablePreferenceAtomic.h',Path(__file__).resolve()]
 (a.output/'receipt.json').write_text(json.dumps({'suite':'actual Quick controller staging and session/radio paths','runtime_provider_double':True,'hardware_tested':False,'sanitized':a.sanitize,'cases':results,'compile_command':cmd,'rgb_adapter_compile_command':rgb_cmd,'sdk_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(inc.glob('*.h'))},'sha256':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
if __name__=='__main__':main()
