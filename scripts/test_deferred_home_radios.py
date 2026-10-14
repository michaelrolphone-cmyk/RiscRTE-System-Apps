#!/usr/bin/env python3
"""Exercise deferred Home Quick radio policy through the real Clock and adapter.

Uses the selected target's build defines and unmodified product sleep sources.
The native doubles reject unexpected capabilities and every call after retention.
Private adapter access is confined to a generated test-only translation unit.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--automatic-provision',action='store_true')
p.add_argument('--candidate', type=Path, required=True)
p.add_argument('--runtime-sdk', type=Path, required=True)
p.add_argument('--sleep-source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve()
include = out / 'include'
include.mkdir(parents=True, exist_ok=True)
receipt = json.loads((a.candidate / 'build-evidence.json').read_text())
shutil.copytree(receipt['idle_policy']['compiled_include_directory'], include, dirs_exist_ok=True)
for header in (ROOT / 'lib/PortableApps/include').glob('*.h'):
    if not (include / header.name).exists() or header.name.startswith(('Portable', 'Contexts')):
        shutil.copyfile(header, include / header.name)
sdk = a.runtime_sdk
if not (sdk / 'RiscRuntimeV1.h').is_file():
    sdk = sdk / 'sdk/app'
for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h', 'RiscFailureEvidenceV1.h'):
    if (sdk / name).is_file():
        shutil.copyfile(sdk / name, include / name)
shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
flags = [f for f in receipt['build_defines'] if f.startswith('-D')]
if a.automatic_provision:flags.append('-DPORTABLE_HOME_PROVISION')
for feature in ('PORTABLE_DESK_DEFER_RADIOS', 'TEST_NATIVE_LANDSCAPE'):
    flag = '-D' + feature
    if flag not in flags:
        flags.append(flag)
for feature in ('PORTABLE_DESK_CLOCK_SPARSE_START', 'PORTABLE_QUICK_RADIOS',
                'PORTABLE_RESIDENT_SHELL_HOST', 'PORTABLE_RESIDENT_LOADING', 'PORTABLE_X4_IDLE_POLICY'):
    assert '-D' + feature in flags, feature
wrapper = out / 'adapter_under_test.c'
wrapper.write_text('#include "' + str(ROOT / 'lib/PortableApps/src/adapter.c') + '"\n' + r'''
/* Test visibility only: every action delegates to the production implementation. */
bool deferred_test_ready(bool force) { return desk_radio_policy_ready(force); }
bool deferred_test_loaded(void) { return desk_radios_loaded; }
bool deferred_test_home(void) { return desk_home_submitted; }
unsigned deferred_test_pending(void) {
 return (paper_token?1u:0u) | (surface.frame?2u:0u)
#ifdef PORTABLE_RASTER_SNAPSHOT
  | (raster_sealed?4u:0u) | (raster_recording?8u:0u)
#endif
 ;
}
void deferred_test_busy(unsigned flags) {
 display_settled=!(flags&1u);paper_token=(flags&2u)?1:0;
 surface.frame=(flags&4u)?1:0;
#ifdef PORTABLE_RASTER_SNAPSHOT
 raster_sealed=!!(flags&8u);raster_recording=!!(flags&16u);
#endif
}
bool deferred_test_mutable(void) { return raster_lease_mutable(); }
bool deferred_test_low_battery(void) { return low_battery_poll(); }
bool deferred_test_queue_token(void) {
 for(unsigned i=0;i<1000&&!paper_token;i++)if(!paper_present_progress())return false;
 return paper_token!=0;
}
bool deferred_test_submit(void) {
 if(!portable_paper_frame_ready())return false;
 portable_desk_adapter_begin();present(false);return !failed;
}
bool deferred_test_quick(void) { bool consumed=false;return quick_foreground(&consumed); }
bool deferred_test_alarm(void) { bool consumed=false;return alarm_foreground(&consumed); }
bool deferred_test_launch(void) { return desk_guard_launch("settings.elf"); }
#ifdef PORTABLE_HOME_PROVISION
void deferred_test_provision(void){home_provision_progress();}
void deferred_test_neutral(void){(void)pqa_input(&quick.ui,millis_now(),true,0,0,0,0,true);}
#endif
''')
names = ('desk_clock_faces.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
         'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c', 'quick_actions.c',
         'quick_render.c', 'quick_session.c', 'quick_radios.c')
sources = [ROOT / 'Apps/paper_clock.c', wrapper,
           *[ROOT / 'lib/PortableApps/src' / name for name in names],
           a.sleep_source, Path(receipt['idle_policy']['source']),
           ROOT / 'test/native_apps/deferred_home_radios_test.c']
if '-DPORTABLE_INSTALLED_CATALOG' in flags:
    sources.append(ROOT / 'lib/PortableApps/src/PortableInstalledCatalog.c')
if a.automatic_provision:
    sources += [ROOT/'lib/PortableApps/src/PortableAutomaticProvision.c',ROOT/'lib/PortableApps/src/PortableNetworkSession.c']
incs = ['-I' + str(include), '-I' + str(ROOT / 'lib/NativeApps/include'),
        '-I' + str(a.sleep_source.parent.parent / 'drivers/x4pro_power')]
cases = ['clock-sync', 'clock-async', 'settled-once', 'blocked-frames', 'timer-never',
         'force-quick', 'force-sleep', 'force-launch', 'force-resident',
         'force-continuation', 'empty-continuation', 'force-pending-frame',
         'alarm-cue-defers', 'alarm-modal-defers', 'force-navigation', 'low-battery-pending', 'retained-before-ready', 'failed-pending-frame']
failures = ['kv-acquire', 'kv-context', 'wifi-acquire', 'ble-acquire', 'wifi-off', 'wifi-status', 'ble-enable',
            'ble-status', 'kv-release', 'wifi-release', 'ble-release']
cases += ['failure-' + failure for failure in failures]
if a.automatic_provision:cases += ['provision-gates','provision-close']
runs = []
for sanitized in (False, True):
    label = 'asan-ubsan' if sanitized else 'normal'
    directory = out / label
    directory.mkdir(exist_ok=True)
    binary = directory / 'fixture'
    extra = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
             '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               *extra, *flags, *incs, *map(str, sources),
               '-Wl,--wrap=portable_desk_adapter_home_submitted', '-Wl,--wrap=pqa_radios_load',
               '-Wl,--wrap=portable_app_alarm_sleep', '-Wl,--wrap=portable_app_idle_sleep',
               '-o', str(binary)]
    subprocess.run(command, check=True)
    (directory / 'compile.json').write_text(json.dumps(command, indent=2) + '\n')
    for case in cases:
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
        result = subprocess.run([binary, case], capture_output=True, text=True, env=env, timeout=30)
        assert result.returncode == 0, (label, case, result.stdout, result.stderr)
        runs.append(dict(profile=label, case=case, result=result.stdout.strip()))
        print(label + ': ' + result.stdout.strip(), flush=True)
tracked = [*sources, ROOT / 'test/native_apps/sparse_clock_startup_test.c',
           ROOT / 'test/native_apps/paper_clock_test.c', ROOT / 'Apps/paper_sparse_clock.inc',
           *[ROOT / 'lib/PortableApps/src' / name for name in ('adapter.c', 'sparse_clock_adapter.inc',
             'quick_adapter.inc', 'resident_shell.inc', 'resident_loading.inc', 'alarm.inc')], Path(__file__)]
(out / 'receipt.json').write_text(json.dumps(dict(
    process_cases=len(runs), runs=runs, build_defines=flags,
    actual_clock_startup=True, actual_adapter=True, actual_quick_radios=True,
    product_sleep_sha256=hashlib.sha256(a.sleep_source.read_bytes()).hexdigest(),
    hardware='not run',
    source_sha256={str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in tracked}), indent=2) + '\n')
print(str(len(runs)) + ' deferred Home Quick radio cases PASS')
