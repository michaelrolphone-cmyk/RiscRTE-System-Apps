#!/usr/bin/env python3
"""Link the builder's actual complete sync source group without section GC.

The negative control deliberately omits the catalog and must fail on the same
helpers that the target import gate rejected. No runtime import allowlist change.
"""
import argparse,hashlib,json,pathlib,shutil,subprocess
from build_portable_wifi import TIME_SYNC_SOURCES
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for folder in ('driver','hardware','app'):
 for source in (a.runtime/'sdk'/folder).glob('*.h'):shutil.copyfile(source,inc/source.name)
commands=[]
base=['cc','-std=c11','-Os','-Wall','-Wextra','-Werror','-fPIC','-shared','-Wl,--no-undefined','-I'+str(inc)]
sources=[ROOT/'lib/PortableApps/src'/name for name in TIME_SYNC_SOURCES]
for negative in (True,False):
 selected=[s for s in sources if not negative or s.name!='PortableTimeZoneCatalog.c'];elf=out/('negative.so' if negative else 'complete.so')
 cmd=[*base,*map(str,selected),'-o',str(elf)];commands.append(cmd);r=subprocess.run(cmd,text=True,capture_output=True)
 (out/('negative.log' if negative else 'complete.log')).write_text(r.stdout+r.stderr)
 if negative:assert r.returncode and 'portable_timezone_count' in r.stderr and 'portable_timezone_get' in r.stderr
 else:
  r.check_returncode();undefined=subprocess.check_output(['nm','-u',str(elf)],text=True)
  assert 'portable_' not in undefined and 'risc_' not in undefined
  (out/'undefined-symbols.txt').write_text(undefined)
(out/'receipt.json').write_text(json.dumps({'passed':True,'target_compiler_invoked':False,'actual_builder_source_group':TIME_SYNC_SOURCES,'negative_control':'missing catalog reproduces count/get unresolved imports','commands':commands,'sha256':{str(s.relative_to(ROOT)):hashlib.sha256(s.read_bytes()).hexdigest() for s in [*sources,ROOT/'scripts/build_portable_wifi.py',pathlib.Path(__file__)]}},indent=2)+'\n')
print('PASS actual sync builder source closure; missing-catalog control rejected; no unresolved portable/runtime helpers')
