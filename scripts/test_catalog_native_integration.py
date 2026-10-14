#!/usr/bin/env python3
"""Run the actual catalog client against the pinned native Runtime owner.

Requires --runtime at the exact .39 native revision. Does not modify Runtime.
"""
import argparse,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PIN='c258c4ba144247b6eaa4c6f28675ffccec582fe1'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args();runtime=a.runtime.resolve()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip()==PIN
with tempfile.TemporaryDirectory(prefix='catalog-native-client-') as temporary:
 temp=Path(temporary)
 client=temp/'actual-client.c'
 client.write_text('''#include "PortableAppCatalog.h"
#include <assert.h>
void actual_catalog_client(const risc_runtime_api_v1 *rt) {
 portable_app_catalog catalog={0};
 assert(portable_catalog_refresh(&catalog,rt));
 assert(catalog.initialized && !catalog.uncertain && catalog.generation && !catalog.failure_stage);
 for(uint32_t i=0;i<catalog.count;i++) {
  assert(catalog.entries[i].manifest.display_name[0] && catalog.entries[i].route[0]);
  portable_app_catalog_launch launch;assert(portable_catalog_launch_prepare(&catalog,i,&launch));
 }
 assert(portable_catalog_close(&catalog,rt));
}
''')
 fixture=(runtime/'test/app_catalog_runtime_test.cpp').read_text()
 marker='static std::string root,mode,longRoute;';assert fixture.count(marker)==1
 fixture=fixture.replace(marker,'extern "C" void actual_catalog_client(const risc_runtime_api_v1*);\n'+marker)
 marker='  reentry=4;acquire();';assert fixture.count(marker)==1
 fixture=fixture.replace(marker,'  if(mode=="normal" || mode=="empty" || mode=="normalized_metadata" || mode=="long_path")actual_catalog_client(rt);\n'+marker)
 test=temp/'fixture.cpp';test.write_text(fixture)
 script=(runtime/'test/run_app_catalog_runtime_test.sh').read_text()
 script=script.replace('repo="$(cd "$(dirname "$0")/.." && pwd)"',f'repo="{runtime}"')
 script=script.replace('flags=();','flags=(-DRISC_NATIVE_SETTINGS_LIFECYCLE=1);').replace('then flags=(', 'then flags+=(')
 compile_client=f'cc "${{flags[@]}}" -std=c11 -Wall -Wextra -Werror -I"{ROOT}/lib/PortableApps/include" -I"{ROOT}/lib/NativeApps/include" -c "{client}" -o "$build/actual-client.o"\n'
 assert script.count('c++ "${flags[@]}"')==1
 script=script.replace('c++ "${flags[@]}"',compile_client+'c++ "${flags[@]}"')
 script=script.replace('"$repo/test/app_catalog_runtime_test.cpp"',f'"{test}" "$build/actual-client.o"')
 run=temp/'run.sh';run.write_text(script)
 for sanitized in (False,True):
  print('Real native owner + actual catalog client; native settings lifecycle enabled; sanitized='+str(sanitized),flush=True)
  subprocess.run(['bash',str(run)],check=True,env={**os.environ,'SANITIZE':'1' if sanitized else '0','ASAN_OPTIONS':'detect_leaks=0'})
print('PASS 16 native ownership scenarios with actual client integration, normal and ASan/UBSan; hardware unverified')
