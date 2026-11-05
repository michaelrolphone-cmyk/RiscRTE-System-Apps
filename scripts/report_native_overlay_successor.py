#!/usr/bin/env python3
"""Compare a native-overlay successor against exact immutable released controls.

This is target-byte software admission and recipe evidence. It does not execute
Xtensa code or claim allocator/heap fit on any physical device.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SELECTORS = {'-DPORTABLE_QUICK_NATIVE', '-DPORTABLE_ALARM_NATIVE'}

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def compare(app, app_dir, memory_path):
    controls = json.loads((ROOT/'docs/native-overlays/released-controls.json').read_text())[app]
    baseline = ROOT/'docs/native-overlays'/('released-'+app)
    for name, expected in controls['assets'].items():
        if sha((baseline/name).read_bytes()) != expected:
            raise ValueError('Immutable released control changed: '+name)
    record_name = app+'-build-record.json'
    old = json.loads((baseline/record_name).read_text())
    new = json.loads((app_dir/record_name).read_text())
    memory_name = 'memory-admission.json' if app == 'settings' else 'elf-memory-admission.json'
    old_memory = json.loads((baseline/memory_name).read_text())
    new_memory = json.loads(memory_path.read_text())
    command = json.loads((app_dir/'command.json').read_text())
    actual = [s for s in command if s.startswith('-D')]
    if actual != new['build_defines']:
        raise ValueError('Successor command and recorded selectors differ')
    version_prefix = '-DPORTABLE_'+('SETTINGS' if app == 'settings' else 'WIFI')+'_VERSION='
    old_flags = {f for f in old['build_defines'] if not f.startswith(version_prefix)}
    new_flags = {f for f in actual if not f.startswith(version_prefix)}
    if old_flags & SELECTORS or new_flags != old_flags | SELECTORS:
        raise ValueError('Successor must preserve every released feature and add exactly both native overlays')
    if old['required_capabilities'] != new['required_capabilities']:
        raise ValueError('Full released capability contract changed')
    if old['exports'] != new['exports'] or old['imports'] != new['imports']:
        raise ValueError('Native overlay target changed the public import/export contract')
    if old['external_sources'] != new['external_sources']:
        raise ValueError('Unrelated external dependency pins changed')
    image = new_memory['images']['native']; prior_image = old_memory['images']['native']
    elf_name = 'settings.elf' if app == 'settings' else 'wifi_settings.elf'
    elf = (app_dir/elf_name).read_bytes()
    if sha(elf) != new['sha256'] or sha(elf) != image['sha256'] or len(elf) != image['file_size_bytes']:
        raise ValueError('Target build and admission receipts describe different bytes')
    loader = image['section_loader']; prior_loader = prior_image['section_loader']
    if not loader['within_structural_limit'] or new_memory['linked_device_memory_fit_qualified']:
        raise ValueError('Structural admission missing or physical fit overstated')
    metrics = ('executable_allocation_request_bytes', 'data_allocation_request_bytes',
               'resident_section_allocation_request_bytes', 'uncached_input_plus_section_allocations_bytes')
    old_sources = old['source_sha256']; new_sources = new['source_sha256']
    return {'schema': 1, 'application': app, 'baseline': controls,
        'successor_source': new['repository_commit'], 'successor_version': new['version'],
        'baseline_elf_sha256': old['sha256'], 'successor_elf_sha256': new['sha256'],
        'baseline_elf_bytes': old['size_bytes'], 'successor_elf_bytes': new['size_bytes'],
        'elf_byte_delta': new['size_bytes']-old['size_bytes'],
        'added_selectors': sorted(SELECTORS), 'all_released_features_preserved': True,
        'capabilities_unchanged': True, 'required_capabilities': new['required_capabilities'],
        'imports_unchanged': True, 'imports': new['imports'], 'exports': new['exports'],
        'external_pins_unchanged': True,
        'allocation_comparison': {k: {'baseline': prior_loader[k], 'successor': loader[k], 'delta': loader[k]-prior_loader[k]} for k in metrics},
        'structural_limit_bytes': loader['structural_limit_bytes'],
        'structural_limit_margin_bytes': loader['structural_limit_bytes']-loader['resident_section_allocation_request_bytes'],
        'source_closure_delta': {'added': sorted(new_sources.keys()-old_sources.keys()),
            'removed': sorted(old_sources.keys()-new_sources.keys()),
            'changed': sorted(k for k in new_sources.keys()&old_sources.keys() if new_sources[k]!=old_sources[k])},
        'hardware_verified': False, 'linked_device_memory_fit_qualified': False,
        'scope': new_memory['admission_scope'], 'unqualified_requirements': new_memory['unqualified_requirements']}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--app',choices=('settings','wifi'),required=True)
    p.add_argument('--app-dir',type=Path,required=True)
    p.add_argument('--memory',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();report=compare(a.app,a.app_dir,a.memory)
    a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(a.app+': both native overlays selected; released features, capabilities, imports/exports and external pins unchanged; allocation delta recorded')
if __name__=='__main__':main()
