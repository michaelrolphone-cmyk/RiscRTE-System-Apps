#!/usr/bin/env python3
"""Fail-closed exact-source native App Store release evidence validation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from build_native_app_store_qualification import (
    ROOT, APP_MANIFEST, CATALOG_PROFILE, PINS, REPOSITORIES, CAPABILITY_SELECTORS,
    RECIPE, RECIPE_SHA256, ALLOWED_IMPORTS, EXPORTS, manifest, validate_recipe)
from report_native_settings_memory import inspect, ADMISSION_SOURCE
from test_native_app_store_opt_out import BASELINE, BASELINE_TREE, PROFILES

VERSION = '1.4.0'
TAG = 'native-app-store-'+VERSION+'-dev1'
SIZES = ((128,128), (160,128), (128,160), (240,240), (410,502), (320,180), (502,240))
CASE_COUNT = 24
# Exact canonical headers at the immutable Runtime pin, including one bounded
# overlay-fixture-only header which is not an application capability.
RUNTIME_HEADER_SHA256 = {'RiscRuntimeV1.h': 'fd7f2a7e817f12a78518176f5a3561b95cfdcfafebe19c980359162a5e29e2b6', 'RiscKeyValueAtomicV1.h': '552b0be1890eced2c688700671832bbd1ec15b3663801d59ae946173f94313c5', 'RiscKeyValueAvailabilityV1.h': '7cf7765c26af74813f86a1a041a0a92495840c883f5e231c391294dd928ad976', 'RiscRealtimeV1.h': '639781f728841cda6d40c3033877436b9e5d59519a4b15e11a9bc99b2365395d', 'RiscKeyValueV1.h': '0e73bf29c402be030baf52f5b2b324f5b872549cdf58aa31f45b72bd70977179'}
MODES = {'normal': (False, False), 'sanitized': (True, False),
         'snapshot': (False, True), 'snapshot-sanitized': (True, True)}
DIRECT = ('app_store.elf', 'app_store.json', 'command.json', 'app-store-build-record.json',
          'elf-layout.json', 'elf-memory-admission.json')


def require(ok, message):
    if not ok:
        raise ValueError('Native App Store evidence: '+message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON key: '+key)
            result[key] = value
        return result
    return json.loads(path.read_text(), object_pairs_hook=unique)


def relative(value):
    p = Path(value)
    require(not p.is_absolute() and '..' not in p.parts and str(p) == value, 'unsafe relative path')
    return p


def validate_sources(sources, root):
    require(isinstance(sources, dict) and sources, 'source closure missing')
    for name, expected in sources.items():
        require(re.fullmatch('[0-9a-f]{64}', expected) is not None, 'invalid source digest')
        require(sha(root/relative(name)) == expected, 'source changed: '+name)


def validate(artifact, source_sha, *, allow_dirty=False, root=ROOT):
    require(re.fullmatch('[0-9a-f]{40}', source_sha) is not None, 'exact source commit required')
    app = artifact/'app'
    record = read(app/'app-store-build-record.json')
    declared = read(app/'app_store.json')
    require(record['repository_commit'] == source_sha, 'target source identity differs')
    require(allow_dirty or record['working_tree_dirty'] is False, 'dirty source cannot publish')
    require(record['version'] == VERSION and record['bundle'] == TAG, 'release version differs')
    require(declared == manifest(VERSION) == read(root/APP_MANIFEST), 'full manifest differs')
    require(record['required_capabilities'] == declared['requires'], 'capabilities differ')
    require(record['capability_source_selectors'] == CAPABILITY_SELECTORS, 'selector contract differs')
    require(record['external_sources'] == {k: {'repository': REPOSITORIES[k], 'commit': p}
                                          for k, p in PINS.items()}, 'external pins differ')
    for name in ('native_ui', 'native_quick', 'native_alarm', 'apps_only_service',
                 'quick_atomic_preferences', 'zero_entry_catalog', 'async_rf_cleanup',
                 'compiler_target_verified', 'software_target_validation',
                 'compiled_version_verified', 'host_fixture_excluded'):
        require(record.get(name) is True, 'missing qualification: '+name)
    require(record['hardware_verified'] is False and record['firmware_or_product_replacement'] is False,
            'unqualified deployment claim')
    command = read(app/'command.json')
    require(sha(app/'command.json') == record['command_sha256'], 'command changed')
    validate_recipe(command, VERSION)
    require([a for a in command if a.startswith('-D')] == record['build_defines'], 'selectors differ')
    require(sha(root/RECIPE) == RECIPE_SHA256, 'retained recipe changed')
    require(sha(app/'app_store.elf') == record['sha256'] and
            (app/'app_store.elf').stat().st_size == record['size_bytes'], 'ELF changed')
    require((VERSION+'\0').encode() in (app/'app_store.elf').read_bytes(), 'compiled version missing')
    require(set(record['exports']) == EXPORTS and set(record['imports']) <= ALLOWED_IMPORTS,
            'target ABI differs')
    layout = inspect(app/'app_store.elf')
    require(layout == read(app/'elf-layout.json') and sha(app/'elf-layout.json') ==
            record['elf_layout_sha256'], 'layout changed')
    sources = {k.removeprefix('system:'): v for k, v in record['source_sha256'].items()
               if k.startswith('system:')}
    validate_sources(sources, root)
    require('Apps/update_native.inc' in sources and CATALOG_PROFILE in sources, 'native input closure missing')
    if not allow_dirty:
        for name, expected in sources.items():
            raw = subprocess.check_output(['git', '-C', str(root), 'show', source_sha+':'+name])
            require(hashlib.sha256(raw).hexdigest() == expected, 'uncommitted source input: '+name)
    for name, expected in record['notice_sha256'].items():
        require(relative(name).parts[0] == 'licenses' and sha(app/name) == expected, 'notice changed')
    require(record['notice_sha256'], 'license notices missing')
    admission = read(app/'elf-memory-admission.json')
    require(admission['application'] == 'app_store' and admission['version'] == VERSION and
            admission['runtime_commit'] == PINS['runtime'] and admission['elf_sha256'] == record['sha256'] and
            admission['elf_layout_sha256'] == record['elf_layout_sha256'] and
            admission['manifest_sha256'] == sha(app/'app_store.json') and
            admission['required_capabilities'] == declared['requires'] and admission['images'] == {'native': layout} and
            admission['linked_device_memory_fit_qualified'] is False, 'admission identity differs')
    require(admission['reporter_sha256'] == sha(root/'scripts/report_native_app_store_memory.py') and
            admission['shared_inspector_sha256'] == sha(root/'scripts/report_native_settings_memory.py') and
            admission['admission_harness_sha256'] == hashlib.sha256(ADMISSION_SOURCE.encode()).hexdigest(),
            'production admission reporter changed')
    runs = admission['admission_runs']
    require(len(runs) == 2 and [r['sanitized'] for r in runs] == [False,True], 'admission modes missing')
    for run in runs:
        result = run['result']
        require(run['image'] == 'native-app-store' and all(result.get(k) is True for k in (
            'ordinary_application_admitted','missing_import_rejected','unsafe_state_rejected','driver_role_rejected')) and
            result['alignment_placements_checked'] > 0 and result['data_allocation_request_bytes'] ==
            layout['section_loader']['data_allocation_request_bytes'] and
            result['linked_device_memory_fit_qualified'] is False, 'admission checks incomplete')
    required_cases = {(w,h,r,c) for w,h in SIZES for r in (0,180) for c in range(CASE_COUNT)}
    controllers = {}
    for mode, (sanitized,snapshot) in MODES.items():
        folder = artifact/'controller'/mode
        receipt = read(folder/'receipt.json')
        require(receipt['status'] == 'PASS' and receipt['sanitized'] is sanitized and
                receipt['snapshot'] is snapshot and receipt['runtime_pin'] == PINS['runtime'] and
                receipt['hardware_tested'] is False, 'controller mode missing: '+mode)
        rows = receipt['cases']
        require(len(rows) == len(required_cases) and
                {(v['width'],v['height'],v['rotation'],v['case']) for v in rows} == required_cases and
                all(v['passed'] is True for v in rows), 'controller case coverage differs: '+mode)
        host = read(folder/'source-hashes.json')
        validate_sources(host, root)
        for name, expected in sources.items():
            if Path(name).suffix in ('.c','.h','.inc') and name.startswith(('Apps/','lib/PortableApps/','lib/NativeApps/')):
                require(host.get(name) == expected, 'controller/target source mismatch: '+name)
        require(sha(folder/'fixture.c') == receipt['fixture_sha256'] and
                sha(folder/'include/RiscRuntimeV1.h') == receipt['runtime_header_sha256'] == RUNTIME_HEADER_SHA256['RiscRuntimeV1.h'], 'controller inputs changed')
        for row in rows:
            path = folder/f"{row['width']}x{row['height']}-{row['rotation']}-{row['case']}.log"
            require(path.is_file(), 'missing raw controller result')
        controllers[mode] = {'receipt_sha256': sha(folder/'receipt.json'),
                             'sources_sha256': sha(folder/'source-hashes.json'), 'cases': len(rows)}
    overlays = {}
    overlay_cases = {(w,h,s,c) for w,h in SIZES for s in (0,1) for c in range(5)}
    for mode, sanitized in (('normal',False),('sanitized',True)):
        folder = artifact/('overlays-'+mode); receipt = read(folder/'receipt.json')
        require(receipt['status'] == 'PASS' and receipt['sanitized'] is sanitized and
                receipt['runtime_pin'] == PINS['runtime'] and receipt['hardware_tested'] is False,
                'actual-app overlays mode missing: '+mode)
        rows = receipt['cases']
        require(len(rows) == len(overlay_cases) and all(row['passed'] is True for row in rows) and
                {(v['width'],v['height'],v['snapshot'],v['case']) for v in rows} == overlay_cases,
                'actual-app overlays matrix differs: '+mode)
        for selector in ('PORTABLE_UPDATE_NATIVE_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_QUICK_NATIVE',
                         'PORTABLE_ALARM_NATIVE','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_QUICK_ATOMIC_PREFERENCES'):
            require(selector in receipt['selected_defines'], 'actual native overlay selector absent: '+selector)
        host = read(folder/'source-hashes.json'); validate_sources(host,root)
        for name, expected in sources.items():
            if Path(name).suffix in ('.c','.h','.inc') and name.startswith(('Apps/','lib/PortableApps/','lib/NativeApps/')):
                require(host.get(name) == expected, 'overlay/target source mismatch: '+name)
        require(sha(folder/'fixture.c') == receipt['fixture_sha256'], 'overlay fixture changed')
        for name in ('RiscRuntimeV1.h','RiscKeyValueAtomicV1.h','RiscKeyValueAvailabilityV1.h','RiscRealtimeV1.h','RiscKeyValueV1.h'):
            require(sha(folder/'include'/name) == RUNTIME_HEADER_SHA256[name], 'overlay Runtime pin header differs')
            if name != 'RiscKeyValueAvailabilityV1.h':
                expected = record['source_sha256'].get('runtime:sdk/app/'+name) or record['source_sha256'].get('system:lib/PortableApps/include/'+name)
                require(expected and sha(folder/'include'/name) == expected, 'overlay/target Runtime header differs')
        for row in rows:
            require((folder/f"{row['width']}x{row['height']}-{row['snapshot']}-{row['case']}.log").is_file(),
                    'missing raw overlay result')
        overlays[mode] = {'receipt_sha256':sha(folder/'receipt.json'),'cases':len(rows)}
    opt = artifact/'opt-out'; witness = read(opt/'receipt.json')
    require(witness['status'] == 'PASS' and witness['baseline_commit'] == BASELINE and
            witness['baseline_tree'] == BASELINE_TREE and witness['candidate_head_commit'] == source_sha and
            witness['candidate_matches_checkout_at_completion'] is True and witness['feature_enabled'] is False,
            'opt-out identity differs')
    require(witness['profile_definitions'] == PROFILES and set(witness['profiles']) ==
            {a+'-'+p for a in ('app_store','ota_update') for p in PROFILES}, 'opt-out matrix differs')
    for row in witness['profiles'].values():
        require(row['identical_bytes'] is True and row['base']['sha256'] == row['candidate']['sha256'] and
                row['base']['bytes'] == row['candidate']['bytes'], 'opt-out bytes differ')
    require(sha(opt/'source-hashes.json') == witness['source_hashes_sha256'], 'opt-out sources changed')
    baseline = read(opt/'source-hashes.json')
    validate_sources(baseline['candidate'], root)
    full = artifact/'recipe-opt-out'; full_receipt = read(full/'receipt.json')
    require(full_receipt['status'] == 'PASS' and full_receipt['baseline_commit'] == BASELINE and
            full_receipt['baseline_tree'] == BASELINE_TREE and full_receipt['candidate_head_commit'] == source_sha and
            full_receipt['recipe_sha256'] == RECIPE_SHA256 and full_receipt['external_pins'] == PINS and
            full_receipt['candidate_matches_checkout_at_completion'] is True and full_receipt['native_ui'] is False and
            set(full_receipt['profiles']) == {'app_store','ota_update'}, 'full retained-recipe controls missing')
    for row in full_receipt['profiles'].values():
        require(row['identical_bytes'] is True and row['base']['sha256'] == row['candidate']['sha256'] and
                row['base']['bytes'] == row['candidate']['bytes'], 'full recipe opt-out bytes differ')
    require(sha(full/'source-hashes.json') == full_receipt['source_hashes_sha256'], 'full recipe source receipt changed')
    validate_sources(read(full/'source-hashes.json')['candidate'],root)
    regressions = artifact/'regressions'; reg = read(regressions/'receipt.json')
    require(reg['status'] == 'PASS' and reg['repository_commit'] == source_sha and reg['normal_and_sanitized'] is True and
            reg['legacy_layout_scenarios'] == 312 and reg['paired_cohort_scenarios_per_mode'] == 46 and
            reg['service_scenarios_per_kind_and_mode'] == 29 and
            reg['controller_scenarios_per_kind_rotation_view_and_mode'] == 38, 'shared fault suites missing')
    require(sha(regressions/'source-hashes.json') == reg['source_sha256'], 'regression sources changed')
    validate_sources(read(regressions/'source-hashes.json'), root)
    for name, expected in reg['log_sha256'].items():
        require(sha(regressions/relative(name)) == expected, 'raw regression log changed')
    return {'schema': 1, 'status': 'PASS', 'source_commit': source_sha, 'tag': TAG,
            'elf_sha256': record['sha256'], 'manifest_sha256': sha(app/'app_store.json'),
            'admission_sha256': sha(app/'elf-memory-admission.json'),
            'controllers': controllers, 'overlays': overlays,
            'full_recipe_opt_out_sha256':sha(full/'receipt.json'), 'opt_out_sha256': sha(opt/'receipt.json'),
            'regressions_sha256': sha(regressions/'receipt.json'),
            'software_only': True, 'hardware_verified': False, 'allow_dirty': allow_dirty}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--source-sha', required=True)
    parser.add_argument('--allow-dirty', action='store_true', help='Local development validation only; cannot publish')
    args = parser.parse_args()
    print(json.dumps(validate(args.artifact.resolve(), args.source_sha, allow_dirty=args.allow_dirty), indent=2))
