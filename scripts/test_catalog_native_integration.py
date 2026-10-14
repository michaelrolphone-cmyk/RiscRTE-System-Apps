#!/usr/bin/env python3
"""Run the actual catalog client against the pinned native Runtime owner.

Exercise all explicit atomic KV/native settings lifecycle flag combinations,
with and without sanitizers. Does not modify Runtime or application sources.
"""
import argparse
from collections import Counter
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PIN = 'a2c2c9fa692b0b36f9750d44fac807c9149b9434'
CONFIGURATIONS = {'default': (0, 0), 'atomic': (1, 0), 'lifecycle': (0, 1), 'atomic-lifecycle': (1, 1)}
CLIENT_MODES = {'normal': 23, 'empty': 0, 'normalized_metadata': 1, 'long_path': 1}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--configuration', choices=CONFIGURATIONS, action='append',
               help='select a feature configuration (repeatable; default: all four)')
a = p.parse_args()
runtime = a.runtime.resolve()
configurations = list(dict.fromkeys(a.configuration or CONFIGURATIONS))
assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=runtime, text=True).strip() == PIN


def replace_once(text, marker, replacement):
    assert text.count(marker) == 1, ('Runtime fixture/runner changed', marker)
    return text.replace(marker, replacement)


with tempfile.TemporaryDirectory(prefix='catalog-native-client-') as temporary:
    temp = Path(temporary)
    client = temp / 'actual-client.c'
    client.write_text('''#include "PortableAppCatalog.h"
#include <assert.h>
#ifndef RISC_RUNTIME_ATOMIC_KEY_VALUE
#error "The client must receive the Runtime feature configuration"
#endif
#ifndef RISC_NATIVE_SETTINGS_LIFECYCLE
#error "The client must receive the Runtime feature configuration"
#endif
void actual_catalog_client(const risc_runtime_api_v1 *rt,const char *mode,uint32_t expected_count) {
 portable_app_catalog catalog={0};
 bool refreshed=portable_catalog_refresh(&catalog,rt);
 if(!refreshed)fprintf(stderr,"ACTUAL_CATALOG_CLIENT FAIL atomic=%d lifecycle=%d mode=%s stage=%s result=%ld uncertain=%d grant_owned=%d healthy_absence=%d\\n",
  RISC_RUNTIME_ATOMIC_KEY_VALUE,RISC_NATIVE_SETTINGS_LIFECYCLE,mode,
  catalog.failure_stage?catalog.failure_stage:"unknown",(long)catalog.failure_result,
  catalog.uncertain,portable_optional_grant_owned(&catalog.grant),
  !catalog.uncertain && portable_optional_missing(rt,&catalog.grant));
 assert(refreshed);
 assert(catalog.initialized && !catalog.uncertain && catalog.generation && !catalog.failure_stage);
 assert(catalog.count==expected_count);
 const uint64_t generation=catalog.generation;
 for(uint32_t i=0;i<catalog.count;i++) {
  assert(catalog.entries[i].manifest.display_name[0] && catalog.entries[i].route[0]);
  portable_app_catalog_launch launch;assert(portable_catalog_launch_prepare(&catalog,i,&launch));
  assert(launch.generation==generation && launch.index==i);
  assert(!strcmp(launch.route,catalog.entries[i].route));
 }
 assert(portable_catalog_refresh(&catalog,rt));
 assert(catalog.count==expected_count && catalog.generation==generation && !catalog.failure_stage);
 assert(portable_catalog_close(&catalog,rt));
 assert(!catalog.api && !catalog.entries && !catalog.staging && !catalog.count && !catalog.initialized);
 assert(portable_catalog_refresh(&catalog,rt));
 assert(catalog.count==expected_count && catalog.generation==generation && !catalog.uncertain);
 assert(portable_catalog_close(&catalog,rt));
 printf("ACTUAL_CATALOG_CLIENT PASS atomic=%d lifecycle=%d mode=%s count=%u\\n",
  RISC_RUNTIME_ATOMIC_KEY_VALUE,RISC_NATIVE_SETTINGS_LIFECYCLE,mode,expected_count);
}
''')
    fixture = (runtime / 'test/app_catalog_runtime_test.cpp').read_text()
    fixture = replace_once(fixture, 'static bool log(const char*){return true;}',
        'static bool log(const char* line){if(!strncmp(line,"APP_CATALOG ",12))fprintf(stderr,"%s\\n",line);return true;}')
    marker = 'static std::string root,mode,longRoute;'
    fixture = replace_once(fixture, marker,
        'extern "C" void actual_catalog_client(const risc_runtime_api_v1*,const char*,uint32_t);\n' + marker)
    marker = '  reentry=4;acquire();'
    calls = ''.join(f'  if(mode=="{mode}")actual_catalog_client(rt,mode.c_str(),{count});\n'
                    for mode, count in CLIENT_MODES.items())
    fixture = replace_once(fixture, marker, calls + marker)
    test = temp / 'fixture.cpp'
    test.write_text(fixture)
    script = (runtime / 'test/run_app_catalog_runtime_test.sh').read_text()
    script = replace_once(script, 'repo="$(cd "$(dirname "$0")/.." && pwd)"',
                          'repo=' + shlex.quote(str(runtime)))
    # Reuse the owner's complete feature matrix without overriding its flags.
    # The actual C client and C++ owner must be built with the same selection.
    compile_client = (f'cc "${{flags[@]}}" "${{features[@]}}" -std=c11 -Wall -Wextra -Werror '
                      f'-I{shlex.quote(str(ROOT / "lib/PortableApps/include"))} '
                      f'-I{shlex.quote(str(ROOT / "lib/NativeApps/include"))} '
                      f'-c {shlex.quote(str(client))} -o "$build/actual-client.o"\n')
    script = replace_once(script, 'c++ "${flags[@]}" "${features[@]}"',
                          compile_client + 'c++ "${flags[@]}" "${features[@]}"')
    script = replace_once(script, '"$repo/test/app_catalog_runtime_test.cpp"',
                          shlex.quote(str(test)) + ' "$build/actual-client.o"')
    run = temp / 'run.sh'
    run.write_text(script)
    expected = Counter((str(atomic), str(lifecycle), mode, str(count))
                       for configuration in configurations for atomic, lifecycle in [CONFIGURATIONS[configuration]]
                       for mode, count in CLIENT_MODES.items())
    for sanitized in (False, True):
        print('Real native owner + actual catalog client; configurations=' + ','.join(configurations) + '; sanitized=' + str(sanitized), flush=True)
        result = subprocess.run(['bash', str(run)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                env={**os.environ, 'SANITIZE': '1' if sanitized else '0',
                                     'APP_CATALOG_CONFIGURATIONS': ' '.join(configurations), 'ASAN_OPTIONS': 'detect_leaks=0'})
        print(result.stdout, end='', flush=True)
        result.check_returncode()
        observed = Counter(re.findall(r'^ACTUAL_CATALOG_CLIENT PASS atomic=([01]) lifecycle=([01]) mode=(\w+) count=(\d+)$',
                                      result.stdout, re.MULTILINE))
        assert observed == expected, ('actual client matrix incomplete', observed, expected)
print(f'PASS {16 * len(configurations)} native ownership scenarios and {8 * len(configurations)} actual client checks across {len(configurations)} feature configurations, normal and ASan/UBSan; hardware unverified')
