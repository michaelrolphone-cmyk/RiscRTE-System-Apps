#!/usr/bin/env python3
"""Qualify native Wi-Fi ELF admission with pinned production Runtime helpers.

Reports exact section allocation requests and synthetic host checks only.
Does not relocate or execute Xtensa code or qualify physical-device heap fit.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

import report_native_settings_memory as shared
from build_native_wifi_qualification import manifest as expected_manifest

RUNTIME_PIN = shared.RUNTIME_PIN
RUNTIME_INPUTS = shared.RUNTIME_INPUTS
ADMISSION_SOURCE = shared.ADMISSION_SOURCE


def matching_layout(elf, layout_path):
    """The required builder receipt must describe these exact target bytes."""
    layout_bytes = layout_path.read_bytes()
    layout = json.loads(layout_bytes)
    observed = shared.inspect(elf)
    shared.require(layout == observed, 'Required elf-layout.json differs from the target ELF')
    return observed, shared.sha(layout_bytes)


def qualify(args):
    out = args.output.resolve()
    shared.require(not out.exists() or not any(out.iterdir()),
                   'Refuse stale evidence: output directory must be empty')
    reporter_sources = {Path(__file__): shared.sha(Path(__file__).read_bytes()),
                        Path(shared.__file__): shared.sha(Path(shared.__file__).read_bytes())}
    elf = args.elf.resolve()
    layout_path = (args.elf_layout or elf.with_name('elf-layout.json')).resolve()
    layout, layout_sha = matching_layout(elf, layout_path)
    manifest_path = (args.manifest or elf.with_name('wifi_settings.json')).resolve()
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    shared.require(manifest == expected_manifest('1.4.0'),
                   'Required native Wi-Fi manifest differs from the complete versioned recipe')
    manifest_sha = shared.sha(manifest_bytes)
    out.mkdir(parents=True, exist_ok=True)
    runtime = out/'runtime'; hashes = {}
    for relative in RUNTIME_INPUTS:
        content = subprocess.check_output(['git', '-C', str(args.runtime), 'show', RUNTIME_PIN+':'+relative])
        dest = runtime/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(content)
        hashes[relative] = shared.sha(content)
    (out/'admit.cpp').write_text(ADMISSION_SOURCE)
    include = ['-I'+str(runtime/'test/native_apps/stubs'),
               '-I'+str(runtime/'lib/elf_loader/include'), '-I'+str(runtime/'src')]
    runs = []
    for sanitized in (False, True):
        flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer', '-no-pie']
        obj = out/f'validate-{int(sanitized)}.o'
        binary = out/f'admit-{int(sanitized)}'
        commands = [
            [os.environ.get('CC', 'cc'), '-std=c11', *flags, *include, '-c',
             str(runtime/RUNTIME_INPUTS[0]), '-o', str(obj)],
            [os.environ.get('CXX', 'c++'), '-std=c++17', *flags, *include,
             str(out/'admit.cpp'), str(obj), '-o', str(binary)],
        ]
        for command in commands:
            subprocess.run(command, check=True, timeout=120)
        result = subprocess.run([str(binary), str(elf)], capture_output=True,
            text=True, check=True, timeout=30,
            env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        observed = json.loads(result.stdout)
        shared.require(all(observed.get(name) is True for name in (
            'ordinary_application_admitted', 'missing_import_rejected',
            'unsafe_state_rejected', 'driver_role_rejected')),
            'Pinned production application admission or rejection checks failed')
        shared.require(observed.get('alignment_placements_checked', 0) > 0,
                       'Pinned allocation placement checks are absent')
        shared.require(observed['data_allocation_request_bytes'] ==
                       layout['section_loader']['data_allocation_request_bytes'],
                       'ELF layout differs from pinned production allocation helper')
        (out/f'native-{int(sanitized)}.json').write_text(result.stdout)
        runs.append({'image': 'native-wifi', 'sanitized': sanitized,
                     'commands': commands, 'result': observed})
        obj.unlink(); binary.unlink()
    # A concurrent target rebuild must not inherit receipts for old ELF bytes.
    current_layout, current_sha = matching_layout(elf, layout_path)
    shared.require(current_layout == layout and current_sha == layout_sha,
                   'Target ELF or required layout receipt changed during qualification')
    shared.require(shared.sha(manifest_path.read_bytes()) == manifest_sha,
                   'Native Wi-Fi manifest changed during qualification')
    for relative, expected in hashes.items():
        shared.require(shared.sha((runtime/relative).read_bytes()) == expected,
                       'Pinned Runtime input changed during qualification: '+relative)
    for path, expected in reporter_sources.items():
        shared.require(shared.sha(path.read_bytes()) == expected,
                       'Memory reporter changed during qualification: '+path.name)
    report = {
        'schema': 1, 'application': manifest['id'], 'version': manifest['version'],
        'manifest_sha256': manifest_sha, 'required_capabilities': manifest['requires'],
        'purpose': 'standalone-development-software-qualification-only',
        'runtime_commit': RUNTIME_PIN, 'runtime_source_sha256': hashes,
        'reporter_sha256': reporter_sources[Path(__file__)],
        'shared_inspector_sha256': reporter_sources[Path(shared.__file__)],
        'admission_harness_sha256': shared.sha(ADMISSION_SOURCE.encode()),
        'elf_layout_sha256': layout_sha, 'elf_sha256': layout['sha256'],
        'images': {'native': layout}, 'admission_runs': runs,
        'admission_scope': 'Pinned production structural validator, ordinary import/role/lifecycle '
            'helper and alignment-reservation/placement helpers; no relocation, Xtensa execution, '
            'native grant binding or device heap test',
        'dynamic_sleep_snapshot_bytes': '2 * native_width * native_height, separate from loaded sections',
        'linked_device_memory_fit_qualified': False,
        'unqualified_requirements': ['selected linked-loader configuration',
            'allocator capabilities and fragmentation', 'concurrent Runtime/provider/cached-image memory',
            'app stacks and dynamic allocations', 'frame/Quick Controls/sleep buffers',
            'linked public import implementations and ABI'],
    }
    (out/'elf-memory-admission.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Native Wi-Fi: pinned production ELF admission and allocation placement passed '
          '(normal and ASan/UBSan); physical-device memory fit remains unqualified')
    return report


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--elf-layout', type=Path,
                        help='Required builder layout receipt; defaults to elf-layout.json beside --elf')
    parser.add_argument('--manifest', type=Path,
                        help='Required native manifest; defaults to wifi_settings.json beside --elf')
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New or empty evidence directory')
    return parser


if __name__ == '__main__':
    qualify(argument_parser().parse_args())
