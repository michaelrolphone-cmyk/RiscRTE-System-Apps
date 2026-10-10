#!/usr/bin/env python3
"""Actual native Settings desk direction selection, save/reopen and failures."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
import build_portable_settings as settings
ROOT=Path(__file__).resolve().parents[1]
CASES=['save-right','save-left','save-reopen','cancel','home','write-fail','readback-fail','write-retry','unavailable','invalid']
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--settings-build',type=Path,default=ROOT/'build/core-paper-motion/settings')
 p.add_argument('--output-dir',type=Path,default=ROOT/'build/native-desk-direction')
 a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
 include=a.settings_build.resolve()/'performance-sdk/include'
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 receipt={'hardware':'not run','publication':'none','runs':{}}
 for short in [False,True]:
  for sanitized in [False,True]:
   label=('short' if short else 'paper')+('-asan-ubsan' if sanitized else '-normal');binary=out/label
   flags=['-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_SETTINGS_QUICK','-DALARM_SERVICE_TAGGED_V2']
   if short:flags+=['-DTEST_NATIVE_SETTINGS_SHORT']
   if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   sources=[ROOT/'Apps/settings_native_entry.c',ROOT/'test/native_apps/native_desk_direction_test.c',*[ROOT/s for s in settings.NATIVE_TIME_SOURCES],*[ROOT/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]]
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-Wl,--wrap=free','-o',str(binary)],check=True)
   results=[];frames=out/(label+'-frames');frames.mkdir(exist_ok=True)
   with (out/(label+'.log')).open('w') as log:
    for case in CASES:
     result=subprocess.run([binary,case],env=dict(env,NATIVE_SETTINGS_FRAMES=str(frames)),check=True,text=True,stdout=subprocess.PIPE,timeout=20)
     log.write(result.stdout);results.append(json.loads(result.stdout))
   receipt['runs'][label]=results;print(label+': '+str(len(results))+' actual Settings direction cases passed',flush=True)
 receipt['source_sha256']={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in ['scripts/test_native_desk_direction.py','test/native_apps/native_desk_direction_test.c','lib/PortableApps/src/settings.inc','lib/PortableApps/src/settings_view.inc','lib/PortableApps/src/settings_paper.inc','lib/PortableApps/include/PortableDeskClockSettings.h']}
 receipt['source_commit']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 receipt['working_tree_dirty']=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())
 (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()
