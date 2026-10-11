#!/usr/bin/env python3
"""Production adapter lifecycle fixtures; no real providers or hardware.

Use a canonical local driver SDK and the pinned Xtensa compiler. Temp files
are discarded. Compare opt-out token streams and linked adapter harness ELFs
against a supplied base commit, without building/publishing a product image.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--runtime-sdk',type=Path,required=True,help='Canonical Runtime app SDK with invocation-retention suffix')
p.add_argument('--xtensa-cc',required=True)
p.add_argument('--base',default='d52a74bfb4acc01cc3f0a9dda2c95ba2ba679ee9')
a=p.parse_args()
FLAGS=['-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_ALARM_CLIENT',
 '-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_INPUT_NAVIGATION',
 '-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_APP_OWNS_TOUCH_CHROME',
 '-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS']
QUICK=[ROOT/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]
def run(command,**kwargs):
 return subprocess.run(list(map(str,command)),check=True,**kwargs)
with tempfile.TemporaryDirectory(prefix='sparse-adapter-') as directory:
 out=Path(directory);include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include)
 shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
 shutil.copyfile(a.runtime_sdk/'RiscRuntimeV1.h',include/'RiscRuntimeV1.h')
 for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h'):
  shutil.copyfile(a.sdk/name,include/name)
 includes=['-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(ROOT/'lib/PortableApps/src')]
 cases=[(i,) for i in range(5)]+[(i,n) for i in (5,6) for n in range(1,10)]+[(7,n) for n in range(3,10)]+[(8,)]+[(9,n) for n in (2,4,5)]+[(10,n) for n in range(1,6)]+[(i,) for i in range(11,16)]+[(16,n) for n in range(1,5)]+[(i,) for i in range(17,29)]
 for san in (False,True):
  binary=out/'fixture'
  sanitizers=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sanitizers,*FLAGS,*includes,
       ROOT/'test/native_apps/sparse_clock_adapter_test.c',*QUICK,'-o',binary])
  for case in cases:run([binary,*map(str,case)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=20)
  print(f'Production sparse adapter: {len(cases)} {"ASan/UBSan" if san else "normal"} lifecycle fixture cases passed',flush=True)
 version=subprocess.check_output([a.xtensa_cc,'--version'],text=True).splitlines()[0]
 assert '8.4.0' in version and '2021r2-patch5' in version,version
 harness=out/'harness.c';harness.write_text('''#include "PortableApps.h"
#include "PortableDeskClockApp.h"
#include "RiscBatteryGaugeV1.h"
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
int portable_desk_clock_mode(void){return 0;}
bool portable_desk_clock_time(uint8_t*h,uint8_t*m){(void)h;(void)m;return false;}
int portable_app_alarm_sleep(const risc_runtime_api_v1 *r,const risc_display_output_api_v1 *d,const risc_battery_gauge_api_v1 *g,const alarm_service_v1 *a){(void)r;(void)d;(void)g;(void)a;return 0;}
__attribute__((visibility("default"))) void app_main(void){}
''')
 mapping=out/'exports.map';mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
 link=[a.xtensa_cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden',
 '-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv',
 '-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror']
 adapter=ROOT/'lib/PortableApps/src/adapter.c'
 validator=out/'validate-elf'
 run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
      '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
      ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
 for feature_flags in (FLAGS,[f for f in FLAGS if f not in ('-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS')],
                       [f for f in FLAGS if f!='-DPORTABLE_INPUT_NAVIGATION']):
  run([*link,*feature_flags,*includes,adapter,harness,*QUICK,'-lgcc','-o',out/'sparse.elf'])
  run([validator,out/'sparse.elf'],stdout=subprocess.DEVNULL)
  symbols=subprocess.check_output([a.xtensa_cc.removesuffix('gcc')+'nm','-D',str(out/'sparse.elf')],text=True)
  imports={line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
  exports={line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')}
  assert imports<={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','strcpy','malloc','free'},imports
  assert exports=={'app_main','app_module_init','app_module_fini'},exports
 baseline=out/'baseline'/'adapter.c';baseline.parent.mkdir()
 try:
  baseline.write_bytes(subprocess.check_output(['git','show',a.base+':lib/PortableApps/src/adapter.c'],cwd=ROOT))
  for name,flags in [('watch',['-DPORTABLE_NOVA_UI','-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_INPUT_NAVIGATION']),
                     ('paper',[f for f in FLAGS if f not in ('-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_DESK_CLOCK')]+['-DPORTABLE_CROWN_SLEEP_LOCAL'])]:
   tokens=[];hashes=[]
   for source in (baseline,adapter):
    pre=subprocess.check_output([a.xtensa_cc,'-std=c11','-E','-P',*flags,*includes,str(source)])
    # Preserve literal contents and token boundaries when ignoring formatting.
    tokens.append(re.findall(rb""""(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_][A-Za-z_0-9]*|[0-9][A-Za-z_0-9.]*|>>=|<<=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\^=|\|=|##|\.\.\.|[^\s]""",pre))
    run([*link,*flags,*includes,source,harness,*QUICK,'-lgcc','-o',out/'unchanged.elf'])
    hashes.append(hashlib.sha256((out/'unchanged.elf').read_bytes()).hexdigest())
   assert tokens[0]==tokens[1],name+' preprocessor changed'
   assert hashes[0]==hashes[1],name+' ELF bytes changed'
   print(name+' opt-out: identical preprocessed tokens and linked ELF sha256 '+hashes[0],flush=True)
 finally:baseline.unlink(missing_ok=True)
 print('Pinned Xtensa sparse adapter link passed: '+version)
