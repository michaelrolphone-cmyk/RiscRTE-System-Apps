#!/usr/bin/env python3
"""Build isolated Paper Space default.elf. Does not deploy or replace Reader UI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'Apps/paper_space'


def catalog_items(path):
    items = json.loads(Path(path).read_text())
    if not isinstance(items, list) or len(items) > 8:
        raise ValueError('Catalog must be an array of at most eight explicit app entries')
    seen = set()
    for item in items:
        if not isinstance(item, dict) or set(item) != {'label', 'path'}:
            raise ValueError('Each catalog entry requires exactly label and path')
        label, target = item['label'], item['path']
        if not isinstance(label, str) or not 1 <= len(label) <= 48 or any(not 32 <= ord(c) <= 126 for c in label):
            raise ValueError('Labels must contain 1..48 printable ASCII characters')
        if (not isinstance(target, str) or not 5 <= len(target) <= 192 or target == 'default.elf' or
                '..' in target or not target.endswith('.elf') or
                any(not re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]*', part) for part in target.split('/'))):
            raise ValueError('Expected a normalized store-relative child ELF path')
        if target in seen:
            raise ValueError('Duplicate child path')
        seen.add(target)
    return items


def c_string(value):
    # C11 trigraphs run before string escape interpretation. Escape every '?'
    # so an ordinary catalog label can never form a source-level trigraph.
    return json.dumps(value).replace('?', r'\?')


def build(args):
    items = catalog_items(args.catalog)
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        cc = str(Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio')) /
                 'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((APP / 'manifest.json').read_text())
    if manifest['version'] != '1.0.0':
        raise ValueError('Update the Paper Space source version and regression expectations together')
    catalog = out / 'catalog.c'
    rows = ['{' + c_string(i['label']) + ',' + c_string(i['path']) + '}' for i in items]
    catalog.write_text('#include "PaperSpace.h"\nconst paper_space_item paper_space_items[] = {' +
                       ','.join(rows or ['{"",""}']) + '};\nconst unsigned paper_space_item_count = ' + str(len(items)) + ';\n')
    defines = []
    manifest['requires'] = [{'capability': 'display.output', 'api': 1}]
    if args.input in ('navigation', 'both'):
        defines += ['-DPAPER_SPACE_NAVIGATION']
        manifest['requires'].append({'capability': 'input.navigation', 'api': 1})
    if args.input in ('touch', 'both'):
        defines += ['-DPAPER_SPACE_TOUCH']
        manifest['requires'].append({'capability': 'input.touch.raw', 'api': 1})
    exports = {'app_main', 'app_module_init', 'app_module_fini'}
    version_map = out / 'paper-space.map'
    version_map.write_text('{ global: ' + '; '.join(sorted(exports)) + '; local: *; };\n')
    layout = out / 'abi-layout.c'
    layout.write_text('''#include "sdk/RiscRuntimeV1.h"
#include "sdk/RiscDisplayOutputV1.h"
#include "sdk/RiscInputNavigationV1.h"
#include "sdk/RiscTouchV1.h"
_Static_assert(sizeof(void*) == 4, "32-bit Xtensa ABI");
_Static_assert(sizeof(risc_runtime_api_v1) == 32, "runtime SDK layout");
_Static_assert(sizeof(risc_runtime_capability_v1) == 16, "grant layout");
_Static_assert(sizeof(risc_display_output_api_v1) == 40, "display SDK layout");
_Static_assert(sizeof(risc_input_navigation_api_v1) == 24, "navigation SDK layout");
_Static_assert(sizeof(risc_touch_api_v1) == 32, "touch SDK layout");
''')
    elf = out / 'default.elf'
    subprocess.run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
                    '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared',
                    '-Wl,--hash-style=sysv', '-Wl,--version-script=' + str(version_map),
                    '-Wall', '-Wextra', '-Werror', *defines, '-I' + str(APP),
                    str(APP / 'main.c'), str(catalog), str(layout), '-o', str(elf)], check=True, timeout=120)
    symbols = subprocess.check_output([cc.removesuffix('gcc') + 'nm', '-D', str(elf)], text=True)
    imports = {s.split()[-1] for s in symbols.splitlines() if ' U ' in ' ' + s}
    if not imports <= {'risc_runtime_get_api', 'memcpy', 'memset', 'strcmp'}:
        raise ValueError('Unexpected firmware/runtime imports: ' + str(imports))
    observed = {s.split()[-1] for s in symbols.splitlines() if len(s.split()) >= 3 and s.split()[-2] in ('T', 'D', 'B', 'R')}
    if observed != exports:
        raise ValueError('Unexpected exports: ' + str(observed))
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected ELF32 little-endian Xtensa ET_DYN')
    validator = out / 'validate-elf'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'test/native_apps/stubs'), '-I' + str(ROOT / 'lib/elf_loader/include'),
                    str(ROOT / 'lib/elf_loader/src/esp_elf_validate.c'), str(ROOT / 'test/native_apps/validate_test.c'),
                    '-o', str(validator)], check=True, timeout=60)
    subprocess.run([str(validator), str(elf)], check=True, timeout=60)
    (out / 'paper-space.json').write_text(json.dumps(manifest, indent=2) + '\n')
    # A fragment deliberately omits any invented board/driver/physical-instance
    # assignments. A real deployment must merge its own provider selections.
    fragment = {'default_app': 'default.elf', 'app_capabilities': [{'manifest': 'paper-space.json',
                'grants': [{'capability': r['capability'], 'api': r['api'], 'instance_id': 0} for r in manifest['requires']]}]}
    (out / 'boot-selection.fragment.json').write_text(json.dumps(fragment, indent=2) + '\n')
    inputs = sorted(p for p in APP.rglob('*') if p.is_file()) + [Path(__file__).resolve()]
    record = {'purpose': 'development-entry-slice-not-device-deployment', 'id': manifest['id'],
              'version': manifest['version'], 'input_profile': args.input, 'catalog': items,
              'repository_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'working_tree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
              'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
              'sha256': hashlib.sha256(data).hexdigest(), 'size_bytes': len(data),
              'imports': sorted(imports), 'exports': sorted(exports),
              'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
    (out / 'build-record.json').write_text(json.dumps(record, indent=2) + '\n')
    shutil.copyfile(APP / 'fonts/UFL.txt', out / 'FONT-LICENSE.txt')
    shutil.copyfile(APP / 'fonts/COPYRIGHT.txt', out / 'FONT-COPYRIGHT.txt')
    shutil.copyfile(APP / 'SOURCES.json', out / 'SOURCES.json')
    shutil.copyfile(ROOT / 'LICENSE', out / 'LICENSE.txt')
    print('Paper Space 1.0.0: target ABI, import/export boundaries and real ELF validator PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, default=APP / 'catalog.example.json')
    parser.add_argument('--input', choices=['navigation', 'touch', 'both'], default='navigation')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist/paper-space')
    build(parser.parse_args())
