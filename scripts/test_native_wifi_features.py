#!/usr/bin/env python3
"""Run named native Wi-Fi controller seams using the existing provider fixtures.

Flags are selected from the immutable delivered recipe, never a second product
recipe. Legacy KV and atomic-only fixtures deliberately omit unavailable provider
groups; every omission is recorded. The availability seam enables the delivered
atomic/availability combination. Target ELF qualification covers full composition.
Only staged fixture copies are instrumented; production sources are unchanged.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from build_native_wifi_qualification import ROOT, RECIPE as RECIPE_PATH, RECIPE_SHA256, DEFINES

NATIVE_SWITCHES = {'PORTABLE_WIFI_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS',
                   'PORTABLE_NATIVE_CUSTODY_FENCE', 'PORTABLE_NOVA_UI',
                   'PORTABLE_WIFI_STORAGE_INSTANCE'}
FIXTURE_SWITCHES = {'PORTABLE_WIFI_SETTINGS_APP', 'PORTABLE_WIFI_INSTANCE',
                    'PORTABLE_ALARM_CLIENT', 'PORTABLE_APP_SLEEP_LOCAL',
                    'PORTABLE_INPUT_NAVIGATION', 'WIFI_RETURN_APP',
                    'PORTABLE_TEXT_INPUT_CLIENT', 'PORTABLE_WIFI_PROFILES'}
ATOMIC_SWITCHES = {'PORTABLE_WIFI_ATOMIC_EXPERIMENTAL', 'PORTABLE_WIFI_REMEMBER_CONNECTED'}
HEADER = '''#include <stdbool.h>
static unsigned feature_provider_calls,feature_retains;
static bool feature_retain(void){++feature_retains;return true;}
#define TEST_WIFI_RUNTIME_BINDINGS .retain_invocation=feature_retain,
'''
FENCE = '''
static void feature_retained_fence(void){
 assert(feature_retains==1&&portable_adapter_retained());
 unsigned before=feature_provider_calls;
 wifi_connect();wifi_scan_start();wifi_profiles_step();wifi_tick();
 assert(!wifi_disconnect()&&!wifi_open()&&!portable_wifi_suspend()&&!portable_wifi_close());
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 wifi_storage_step();wifi_storage_reload();
#endif
 t5_app_input_t in;assert(!wa->poll(&in,0));app_main();
 assert(feature_provider_calls==before&&feature_retains==1);
 puts("Native terminal custody: zero subsequent provider calls PASS");
}
'''
ATOMIC_MAIN = '''
int main(int argc,char**argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);
 if(which!=12)return feature_original_atomic(argc,argv);
 atomic_fixture_api=(risc_key_value_atomic_v1){1,sizeof(atomic_fixture_api),NULL,atomic_state,atomic_get,atomic_submit,atomic_collect,atomic_cancel};
 workflow_start(false);assert(portable_wifi_close());kv_denied=true;
 assert(!wifi_open()&&!wifi_store&&!writes&&!submits);
 feature_retained_fence();return 0;
}
'''
AVAILABILITY_MAIN = '''
int main(int argc,char**argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);
 if(which!=9&&which!=13&&which!=14&&which!=17)return feature_original_availability(argc,argv);
 current_availability=RISC_KEY_VALUE_AVAILABLE;
 if(which==17)kv_denied=true;
 availability_start();
 if(which==17){
  assert(opened&&!wifi_store&&!portable_adapter_retained());draft();draft_dirty=true;
  wifi_activate(6);assert(!profile_work&&!submits&&!writes);
  wifi_connect();assert(connects==1&&!remember_join);assert(wifi_disconnect());
  kv_denied=false;wifi_storage_discard();wifi_page(WP_ROOT);wifi_storage_reload();
  assert(wifi_storage_ready()&&wifi_store&&!portable_adapter_retained());
  finish();assert(!availability_data_grants&&!availability_legacy_grants);
  puts("Availability-enabled clean namespace-6 refusal, temporary connection and reload recovery PASS");return 0;
 }
 if(which==9){
  draft();draft_dirty=true;invalid_snapshot=true;wifi_activate(6);
  assert(!wifi_availability_valid&&!profile_work&&!submits&&!writes&&draft_dirty);
 }else if(which==13){
  availability_release_denied=true;change_availability(RISC_KEY_VALUE_RECONCILIATION_PENDING);advance();
  assert(wk.api&&wifi_atomic_api&&availability_data_grants==1&&!availability_data_releases&&!submits&&!writes);
 }else{
  draft();draft_dirty=true;wifi_activate(6);prepared();assert(job_state==1);
  invalid_snapshot=true;unsigned before=collects;wifi_profiles_step();
  assert(collects==before&&job_state==1&&wifi_atomic_edit.token&&!writes);
 }
 feature_retained_fence();return 0;
}
'''
ADAPTATIONS = {
    'atomic:12': 'Atomic-only fixture omits availability: missing store is explicitly terminal at wifi_open. Availability-enabled clean refusal is covered separately by availability:17.',
    'availability:9': 'Malformed namespace snapshot retains the invocation and draft; legacy unconditional disconnect is inapplicable.',
    'availability:13': 'Failed release retains the exact live grant; clearing the fake error cannot resume this invocation.',
    'availability:14': 'Malformed namespace snapshot with an outstanding atomic token retains it; later fake worker completion cannot resume collection.',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes():
    paths = [p for name in ('Apps', 'lib', 'test/native_apps')
             for p in (ROOT/name).rglob('*') if p.is_file()]
    paths += [Path(__file__), ROOT/RECIPE_PATH]
    return {str(p.relative_to(ROOT)): sha(p) for p in sorted(paths)}


def recipe_switches():
    raw = (ROOT/RECIPE_PATH).read_bytes()
    if hashlib.sha256(raw).hexdigest() != RECIPE_SHA256:
        raise ValueError('Pinned delivered Wi-Fi recipe changed')
    flags = [v for v in json.loads(raw)['wifi_settings']['command'] if v.startswith('-D')]
    flags += ['-D'+name for name in DEFINES if '-D'+name not in flags]
    selected = {v[2:].split('=', 1)[0]: v for v in flags}
    for name in NATIVE_SWITCHES | FIXTURE_SWITCHES | ATOMIC_SWITCHES | {'PORTABLE_SETTINGS_AVAILABILITY'}:
        if selected.get(name) is None:
            raise ValueError('Required delivered controller switch absent: '+name)
    return selected


def stage(out):
    """Keep fixture logic, adding provider counters, bindings and named wrappers."""
    names = ('portable_wifi_test.c', 'wifi_workflow_test.c',
             'wifi_atomic_workflow_test.c', 'wifi_availability_test.c')
    for name in names:
        text = (ROOT/'test/native_apps'/name).read_text()
        for dependency in names:
            text = text.replace('"'+dependency+'"', '"'+str(out/dependency)+'"')
        for dependency in ('lib/PortableApps/src/adapter.c', 'Apps/wifi_settings.c'):
            text = text.replace('"../../'+dependency+'"', '"'+str(ROOT/dependency)+'"')
        # Count every synthetic capability entry, including callbacks that do
        # not call observed(). Nested provider calls may count twice; a zero
        # delta after retention still proves none were invoked.
        pattern = r'((?:static )?[^\n;{}]+?\b(?:fake_\w+|text_open|text_poll|text_close|async_begin|async_poll|async_cancel|service_begin|service_end|workflow_acquire|workflow_release|availability_snapshot|availability_atomic_state|availability_atomic_get|atomic_state|atomic_get|atomic_submit|atomic_collect|atomic_cancel|risc_runtime_get_api|portable_app_alarm_sleep)\([^;{}]*\)\s*\{)'
        text, count = re.subn(pattern, r'\1++feature_provider_calls;', text)
        if not count:
            raise ValueError('Fixture provider instrumentation found no callbacks: '+name)
        if name == 'wifi_atomic_workflow_test.c':
            text = text.replace('int main(int argc,char**argv)', 'int feature_original_atomic(int argc,char**argv)')
            # main has an implicit successful return; the renamed function must
            # make that identical C behavior explicit.
            old = 'finish();clean_edit();printf("Atomic Wi-Fi controller case %u PASS\\n",which);'
            if text.count(old) != 1:
                raise ValueError('Atomic fixture main return shape changed')
            text = text.replace(old, old+'return 0;')
        if name == 'wifi_availability_test.c':
            text = text.replace('int main(int argc,char**argv)', 'int feature_original_availability(int argc,char**argv)')
        (out/name).write_text(text)
    for name, fixture, suffix in (
        ('workflow', 'wifi_workflow_test.c', ''),
        ('atomic', 'wifi_atomic_workflow_test.c', FENCE+ATOMIC_MAIN),
        ('availability', 'wifi_availability_test.c', FENCE+AVAILABILITY_MAIN),
    ):
        (out/(name+'.c')).write_text(HEADER+'#include "'+str(out/fixture)+'"\n'+suffix)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    hashes = source_hashes()
    switches = recipe_switches()
    stage(out)
    receipt = {'scope': 'Named controller seams; synthetic providers. Full bridge composition is qualified separately by the 18-capability target ELF.',
               'recipe_sha256': RECIPE_SHA256, 'source_hashes': 'source-hashes.json',
               'sanitized': args.sanitize, 'hardware_tested': False, 'adaptations': ADAPTATIONS,
               'delivered_recipe_defines': list(switches.values()), 'builds': [], 'cases': [], 'status': 'RUNNING'}
    (out/'source-hashes.json').write_text(json.dumps(hashes, indent=2)+'\n')
    receipt['source_hashes_sha256'] = sha(out/'source-hashes.json')
    receipt['staged_fixture_hashes'] = {p.name: sha(p) for p in sorted(out.glob('*.c'))}
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie'] if args.sanitize else []
    includes = ['-I'+str(ROOT/name) for name in ('lib/PortableApps/include', 'lib/NativeApps/include', 'lib/RemoteFiles/vendor/tinydtls_sha2')]
    compiler = shutil.which(os.environ.get('CC', 'cc'))
    if compiler is None:
        raise ValueError('Cannot find selected host compiler')
    cc = str(Path(compiler).resolve())
    receipt['compiler'] = {'path': cc, 'sha256': sha(Path(cc)),
                           'version': subprocess.check_output([cc, '--version'], text=True).splitlines()[0]}
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    try:
        codec = out/'profiles-codec'
        command = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', *flags, *includes,
                   str(ROOT/'test/native_apps/portable_wifi_profiles_test.c'), '-o', str(codec)]
        subprocess.run(command, check=True, timeout=120)
        receipt['profile_codec'] = {'command': command, 'binary_sha256': sha(codec), 'cases': 50}
        run = subprocess.run([str(codec)], capture_output=True, text=True, timeout=30, env=environment)
        (out/'profiles-codec.log').write_text(run.stdout+run.stderr)
        receipt['profile_codec']['returncode'] = run.returncode
        receipt['profile_codec']['log_sha256'] = sha(out/'profiles-codec.log')
        run.check_returncode()
        codec.unlink()
        for name, cases, extra in (
            ('workflow', [*range(25), *range(27,33)], []),
            ('workflow-policy', list(range(33)), ['-DTEST_RADIO_POLICY']),
            ('atomic', list(range(23)), []),
            ('availability', list(range(18)), []),
        ):
            fixture = name.split('-')[0]
            enabled = NATIVE_SWITCHES | FIXTURE_SWITCHES
            if fixture in ('atomic', 'availability'):
                enabled |= ATOMIC_SWITCHES
            if fixture == 'availability':
                enabled |= {'PORTABLE_SETTINGS_AVAILABILITY'}
            provider_files = ['portable_wifi_test.c', 'wifi_workflow_test.c']
            if fixture in ('atomic', 'availability'):
                provider_files.append('wifi_atomic_workflow_test.c')
            if fixture == 'availability':
                provider_files.append('wifi_availability_test.c')
            declarations = {}
            for filename in provider_files:
                for macro, value in re.findall(r'^#define (\w+)([^\n]*)$', (ROOT/'test/native_apps'/filename).read_text(), re.MULTILINE):
                    declarations[macro] = value.strip()
            for macro in enabled-NATIVE_SWITCHES:
                expected = switches[macro][2:].split('=', 1)
                expected = expected[1] if len(expected) == 2 else ''
                actual = declarations.get(macro)
                if actual is not None and re.fullmatch(r'\d+[uU]', actual):
                    actual = actual[:-1]
                if actual != expected:
                    raise ValueError('Fixture differs from delivered recipe: '+macro)
            if name == 'workflow-policy':
                enabled |= {'PORTABLE_QUICK_RADIOS'}
            binary = out/name
            cmd = [cc, '-std=c11', '-DTEST_IDLE_ELIGIBILITY', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags,
                   *[switches[n] for n in sorted(NATIVE_SWITCHES)], *extra, *includes, str(out/(fixture+'.c'))]
            if fixture != 'workflow':
                cmd += [str(ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c')]
            cmd += ['-o', str(binary)]
            preprocess = cmd[:cmd.index(str(out/(fixture+'.c')))] + ['-dM', '-E', str(out/(fixture+'.c'))]
            macros_text = subprocess.check_output(preprocess, text=True, timeout=120)
            (out/(name+'-macros.txt')).write_text(macros_text)
            macros = dict(re.findall(r'^#define (\w+)(?: (.*))?$', macros_text, re.MULTILINE))
            overrides = {}
            for macro in enabled:
                if macro not in macros:
                    raise ValueError('Compiled fixture lacks selected recipe switch: '+macro)
                definition = switches[macro][2:].split('=', 1)
                if len(definition) == 1:
                    if macros[macro] not in ('', '1'):
                        raise ValueError('Compiled boolean differs from recipe: '+macro)
                else:
                    actual = re.sub(r'(?<=\d)[uUlL]+$', '', macros[macro])
                    if actual != definition[1]:
                        raise ValueError('Compiled valued switch differs from recipe: '+macro)
                    if macros[macro] != definition[1]:
                        overrides[macro] = {'recipe': definition[1], 'fixture': macros[macro],
                                            'semantic_value_equal': True}
            receipt['builds'].append({'fixture': name, 'command': cmd,
                'preprocessor_command': preprocess,
                'preprocessor_sha256': sha(out/(name+'-macros.txt')),
                'asserted_recipe_subset': [switches[n] for n in sorted(enabled)],
                'actual_recipe_macros': {n: macros[n] for n in switches if n in macros},
                'fixture_specific_valued_overrides': overrides,
                'omitted_recipe_switches': [v for n,v in switches.items() if n not in macros],
                'omission_reason': 'Fixture supplies only its named controller providers. Product bridges, catalog, checked storage, preferences, sleep, hardware and display deployment flags require the separate target build.'})
            subprocess.run(cmd, check=True, timeout=120)
            receipt['builds'][-1]['binary_sha256'] = sha(binary)
            for case in cases:
                run = subprocess.run([str(binary), str(case)], capture_output=True, text=True, timeout=30, env=environment)
                log = out/(name+'-'+str(case)+'.log')
                log.write_text(run.stdout+run.stderr)
                receipt['cases'].append({'fixture': name, 'case': case, 'returncode': run.returncode,
                                         'log_sha256': sha(log), 'adaptation': ADAPTATIONS.get(fixture+':'+str(case))})
                if run.returncode:
                    print(log.read_text(), end='')
                    run.check_returncode()
            binary.unlink()
            print(name+': '+str(len(cases))+' PASS', flush=True)
        if hashes != source_hashes():
            raise RuntimeError('Production/fixture source changed during qualification')
        receipt['status'] = 'PASS'
    except Exception as error:
        receipt['status'] = 'FAIL'
        receipt['error'] = str(error)
        raise
    finally:
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')


if __name__ == '__main__':
    main()
