#!/usr/bin/env python3
"""Actual resident adapter/cache client; deterministic provider boundaries."""
import argparse,os,shutil,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);inc=out/'include';shutil.copytree(root/'lib/PortableApps/include',inc);shutil.copytree(root/'lib/PortableApps/time',out/'time')
for n in ('RiscRuntimeV1.h','RiscResidentShellV1.h','RiscFailureEvidenceV1.h'):shutil.copyfile(a.runtime/'sdk/app'/n,inc/n)
for n in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h'):shutil.copyfile(a.sdk/n,inc/n)
flags='PORTABLE_PAPER_PREFERENCES PORTABLE_ALARM_TERMINAL_RETENTION PORTABLE_NATIVE_CUSTODY_FENCE PORTABLE_ALARM_CLIENT PORTABLE_INPUT_NAVIGATION PORTABLE_APP_OWNS_TOUCH_CHROME PORTABLE_RESIDENT_SHELL_HOST PORTABLE_QUICK_ACTIONS PORTABLE_PAPER_TRANSITIONS PORTABLE_INSTALLED_CATALOG PORTABLE_INSTALLED_CATALOG_HOST PORTABLE_INSTALLED_VOLUME_INSTANCE=9u PORTABLE_APP_SLEEP_LOCAL'.split()
base=[os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(inc),'-I'+str(root/'lib/NativeApps/include')]
shared=['-Wl,-undefined,dynamic_lookup'] if sys.platform=='darwin' else ['-Wl,-Bsymbolic'];host=[] if sys.platform=='darwin' else ['-rdynamic']
wrapper=out/'adapter.c';wrapper.write_text('#include "'+str(root/'lib/PortableApps/src/adapter.c')+'"\n#include <assert.h>\nvoid test_catalog_sleep_request(void){assert(idle_sleep());}\n')
sources=[wrapper,root/'test/native_apps/home_installed_catalog_app.c',*[root/'lib/PortableApps/src'/n for n in ('PortableInstalledCatalog.c','quick_actions.c','quick_session.c','quick_render.c')]]
subprocess.run([*base,'-fPIC','-shared',*shared,*['-D'+x for x in flags],*map(str,sources),'-o',str(out/'host.elf')],check=True)
subprocess.run([*base,*host,str(root/'test/native_apps/home_installed_catalog_test.c'),'-ldl','-o',str(out/'test')],check=True)
subprocess.run([str(out/'test'),str(out)],check=True)
