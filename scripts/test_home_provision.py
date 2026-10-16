#!/usr/bin/env python3
"""Actual selected sparse Home adapter/controller/network, strict native doubles."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--candidate', type=Path, help='Built Home with build-evidence.json; otherwise use the checked full profile')
p.add_argument('--sdk', type=Path, help='Canonical Reader sdk/driver, required without --candidate')
p.add_argument('--utilities', type=Path, help='Utilities checkout, required without --candidate')
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--x4', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
include = out / 'include'
if a.candidate:
    receipt = json.loads((a.candidate / 'build-evidence.json').read_text())
    shutil.copytree(receipt['idle_policy']['compiled_include_directory'], include, dirs_exist_ok=True)
    flags = [f for f in receipt['build_defines'] if f.startswith('-D')]
else:
    if not a.sdk or not a.utilities: p.error('--sdk and --utilities are required without --candidate')
    shutil.copytree(ROOT / 'lib/PortableApps/include', include, dirs_exist_ok=True)
    for directory in (a.runtime / 'sdk/app', a.x4 / 'minimal/interfaces'):
        for header in directory.glob('*.h'): shutil.copyfile(header, include / header.name)
    for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h', 'RiscTouchV1.h', 'RiscTouchPowerV1.h', 'RiscStorageVolumeV1.h'):
        shutil.copyfile(a.sdk / name, include / name)
    for name in ('RiscLightSleepV1.h', 'RiscTimedSleepV1.h', 'RiscDeepSleepV1.h'):
        shutil.copyfile(a.runtime / 'sdk/driver' / name, include / name)
    import portable_alarm_build
    from types import SimpleNamespace
    portable_alarm_build.stage(SimpleNamespace(tagged_alarm_utilities=a.utilities, alarm_client=True,
        desk_points_face=True, contexts_rf_only=True, sparse_start=True), p, out, include)
    flags = json.loads((ROOT / 'test/native_apps/fixtures/home-provision-profile.json').read_text())
for header in (ROOT / 'lib/PortableApps/include').glob('*.h'):
    if not (include / header.name).exists() or header.name.startswith(('Portable', 'Contexts', 'InstalledApps')):
        shutil.copyfile(header, include / header.name)
for name in ('RiscRuntimeV1.h', 'RiscRealtimeV1.h', 'RiscResidentShellV1.h', 'RiscFailureEvidenceV1.h'):
    shutil.copyfile(a.runtime / 'sdk/app' / name, include / name)
shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
flags = [f for f in flags if not f.startswith('-DPORTABLE_INSTALLED_VOLUME_INSTANCE=')]
for flag in ('-DPORTABLE_HOME_PROVISION', '-DPORTABLE_INSTALLED_VOLUME_INSTANCE=0ULL', '-DTEST_NATIVE_LANDSCAPE', '-DPORTABLE_ALARM_BUSY_DEFER'):
    if flag not in flags: flags.append(flag)
names = ('desk_clock_faces.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
         'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c', 'quick_actions.c',
         'quick_render.c', 'quick_session.c', 'quick_radios.c', 'PortableInstalledCatalog.c',
         'PortableAutomaticProvision.c', 'PortableNetworkSession.c')
sources = [ROOT / 'Apps/paper_clock.c', ROOT / 'test/native_apps/home_provision_adapter.c',
           *[ROOT / 'lib/PortableApps/src' / n for n in names],
           a.x4 / 'minimal/apps/portable_sleep.c', a.x4 / 'minimal/apps/portable_idle_sleep.c',
           ROOT / 'test/native_apps/home_provision_test.c']
wrap = ('portable_desk_adapter_home_submitted', 'pqa_radios_load', 'portable_app_alarm_sleep', 'portable_app_idle_sleep')
cases = ['clock-valid', 'clock-unset', 'gates', 'timer', 'valid', 'unset', 'no-volume', 'airplane', 'wifi-off', 'cancel-pending',
         'quick-pending', 'alarm-pending', 'fini-pending', 'retain-release', 'volume-partial',
         'time-context', 'time-release', 'time-io', 'time-size', 'time-reserved', 'time-bracket',
         'time-nano', 'time-fraction', 'time-validity', 'time-range', 'time-unset-epoch', 'time-unset-nano', 'time-zero']
runs = []
for sanitized in (False, True):
    label = 'asan-ubsan' if sanitized else 'normal'
    binary = out / ('fixture-' + label)
    san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               *san, *flags, '-I' + str(include), '-I' + str(ROOT / 'lib/NativeApps/include'),
               '-I' + str(a.x4 / 'minimal/drivers/x4pro_power'), *map(str, sources),
               *['-Wl,--wrap=' + name for name in wrap], '-o', str(binary)]
    subprocess.run(command, check=True)
    (out / ('compile-' + label + '.json')).write_text(json.dumps(command, indent=2) + '\n')
    for case in cases:
        result = subprocess.run([binary, case], capture_output=True, text=True, timeout=30,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        assert result.returncode == 0, (label, case, result.stdout, result.stderr)
        print(label + ': ' + result.stdout.strip(), flush=True)
        runs.append({'profile': label, 'case': case, 'result': result.stdout.strip()})
tracked = [*sources, Path(__file__), ROOT / 'test/native_apps/fixtures/home-provision-profile.json',
    ROOT / 'test/native_apps/deferred_home_radios_test.c', ROOT / 'test/native_apps/sparse_clock_startup_test.c',
    ROOT / 'test/native_apps/paper_clock_test.c', ROOT / 'Apps/paper_sparse_clock.inc',
    *[ROOT / 'lib/PortableApps/src' / n for n in ('adapter.c', 'home_provision.inc',
      'installed_catalog_adapter.inc', 'sparse_clock_adapter.inc', 'quick_adapter.inc', 'alarm.inc',
      'contexts_policy.inc', 'broadcast_adapter.inc', 'resident_shell.inc', 'resident_loading.inc')]]
(out / 'receipt.json').write_text(json.dumps({'runs': runs, 'process_cases': len(runs), 'build_defines': flags, 'actual_adapter': True,
    'actual_controller': True, 'actual_network_session': True, 'hardware': 'not run',
    'source_sha256': {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in tracked},
    'sdk_sha256': {n: hashlib.sha256((include / n).read_bytes()).hexdigest() for n in
      ('InstalledAppsV1.h', 'RiscRuntimeV1.h', 'RiscRealtimeV1.h')}}, indent=2) + '\n')
print(str(len(runs)) + ' actual Home provisioning cases PASS')
