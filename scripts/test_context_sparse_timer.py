#!/usr/bin/env python3
"""Run the actual sparse TIMER fixture with the selected resident RF Home profile.

Reuses the strict Clock/adapter/product-sleep fixture. Its provider double rejects
unrecognized acquisitions, including contexts.service, and caps live grants at
16. The timer assertions require a 408-byte record and at most six live grants.
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
p.add_argument('--candidate', type=Path, required=True)
p.add_argument('--runtime-sdk', type=Path, required=True)
p.add_argument('--sleep-source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--baseline-pixels', type=Path, help='Existing test_sparse_points_expiry.py proof to compare')
a = p.parse_args()
out = a.output.resolve()
include = out / 'include'
include.mkdir(parents=True, exist_ok=True)
receipt = json.loads((a.candidate / 'build-evidence.json').read_text())
shutil.copytree(receipt['idle_policy']['compiled_include_directory'], include, dirs_exist_ok=True)
for header in (ROOT / 'lib/PortableApps/include').glob('*.h'):
    if not (include / header.name).exists() or header.name.startswith(('Portable', 'Contexts')):
        shutil.copyfile(header, include / header.name)
for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h'):
    shutil.copyfile(a.runtime_sdk / name, include / name)
if (a.runtime_sdk/'RiscFailureEvidenceV1.h').is_file():
    shutil.copyfile(a.runtime_sdk/'RiscFailureEvidenceV1.h',include/'RiscFailureEvidenceV1.h')
shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
flags = [f for f in receipt['build_defines'] if f.startswith('-D')]
for feature in ('PORTABLE_CONTEXTS_CLIENT', 'PORTABLE_CONTEXTS_CLOCK_RF_ONLY',
                'PORTABLE_RESIDENT_POLICY', 'PORTABLE_RESIDENT_LEGACY_HANDOFF', 'TEST_NATIVE_LANDSCAPE'):
    flag = '-D' + feature
    if flag not in flags:
        flags.append(flag)
for feature in ('PORTABLE_DESK_POINTS_SNAPSHOT', 'PORTABLE_RESIDENT_SHELL_HOST', 'PORTABLE_X4_IDLE_POLICY'):
    assert '-D' + feature in flags, feature
names = ('adapter.c', 'desk_clock_faces.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
         'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c', 'quick_actions.c',
         'quick_render.c', 'quick_session.c', 'quick_radios.c')
sources = [ROOT / 'Apps/paper_clock.c', *[ROOT / 'lib/PortableApps/src' / name for name in names],
           a.sleep_source, Path(receipt['idle_policy']['source']),
           ROOT / 'test/native_apps/context_sparse_timer_test.c']
incs = ['-I' + str(include), '-I' + str(ROOT / 'lib/NativeApps/include'),
        '-I' + str(a.sleep_source.parent.parent / 'drivers/x4pro_power')]
cases = ('valid', 'expired', 'rewind', 'old-snapshot', 'step-terminal', 'projection-terminal', 'uncertain')
runs = []
for sanitized in (False, True):
    label = 'asan-ubsan' if sanitized else 'normal'
    directory = out / label
    directory.mkdir(exist_ok=True)
    binary = directory / 'fixture'
    extra = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
             '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    *extra, *flags, *incs, *map(str, sources), '-Wl,--wrap=portable_app_idle_sleep',
                    '-o', str(binary)], check=True)
    def run(case, flip, asynchronous=False, prior=None):
        image = directory / f'{"reboot-" if prior else ""}{case}-{flip}-{int(asynchronous)}.pbm'
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1', EXPIRY_FLIP=flip)
        if asynchronous:
            env['EXPIRY_ASYNC'] = '1'
        if prior:
            env['EXPIRY_RECORD'] = str(prior)
        result = subprocess.run([binary, case, image], capture_output=True, text=True, env=env, timeout=20)
        assert result.returncode == 0, (label, case, flip, asynchronous, result.stdout, result.stderr)
        runs.append(dict(profile=label, case=case, flip=flip, asynchronous=asynchronous,
                         reboot=bool(prior), result=result.stdout.strip()))
        return Path(str(image) + '.record')
    for case in cases:
        for flip in ('0', '1'):
            for asynchronous in (False, True):
                run(case, flip, asynchronous)
    for flip in ('0', '1'):
        prior = directory / f'expired-{flip}-0.pbm.record'
        for case in ('valid', 'expired'):
            prior = run(case, flip, prior=prior)
    print(label + ': RF-only resident Home TIMER, 408-byte reuse and terminal fences PASS', flush=True)
for image in (out / 'normal').glob('*.pbm'):
    assert image.read_bytes() == (out / 'asan-ubsan' / image.name).read_bytes(), image
baseline_matches = 0
if a.baseline_pixels:
    for label in ('normal', 'asan-ubsan'):
        for image in (out / label).glob('*.pbm'):
            name = image.name
            if name.startswith('reboot-'):
                name = name.rsplit('-', 1)[0] + '.pbm'
            before = a.baseline_pixels / label / ('1-' + name)
            assert image.read_bytes() == before.read_bytes(), (image, before)
            assert Path(str(image) + '.record').read_bytes() == Path(str(before) + '.record').read_bytes(), image
            baseline_matches += 1
tracked = [*sources, ROOT / 'test/native_apps/sparse_points_expiry_test.c',
           ROOT / 'test/native_apps/sparse_clock_startup_test.c',
           ROOT / 'Apps/paper_sparse_clock.inc', ROOT / 'Apps/paper_home_points.inc',
           *[ROOT / 'lib/PortableApps/src' / name for name in ('contexts_adapter.inc', 'contexts_policy.inc',
             'contexts_clock.inc', 'sparse_clock_adapter.inc', 'resident_adapter.inc', 'resident_shell.inc',
             'resident_policy.inc')], Path(__file__)]
(out / 'receipt.json').write_text(json.dumps(dict(
    process_cases=len(runs), runs=runs, build_defines=flags, retained_schema=3, retained_bytes=408,
    timer_live_grant_bound=6, runtime_live_grant_bound=16, contexts_timer_acquisitions=0,
    pixels='normal and ASan/UBSan byte identical', hardware='not run',
    baseline_pixels=str(a.baseline_pixels) if a.baseline_pixels else None,
    baseline_matching_images=baseline_matches, baseline_records='408-byte records byte identical' if baseline_matches else None,
    source_sha256={str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in tracked}), indent=2) + '\n')
print(str(len(runs)) + ' actual RF-only resident TIMER cases PASS')
