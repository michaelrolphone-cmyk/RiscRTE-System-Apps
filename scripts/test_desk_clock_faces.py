#!/usr/bin/env python3
"""Test current faces against Reader primitives and the preserved unpadded layout."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / 'lib/PortableApps/include'
ASSETS = ROOT / 'lib/PortableApps/desk_clock'
FIXTURE = ROOT / 'test/fixtures/desk_clock_reader'
HISTORICAL_EVIDENCE = ROOT / 'docs/desk-clock/evidence'
EVIDENCE = ROOT / 'docs/desk-clock/evidence-unpadded'
FACES = ('Segments', 'Sans', 'Serif', 'Minimal', 'Railway', 'Deco')
TIMES = ((0, 0), (0, 1), (0, 59), (1, 0), (9, 5), (9, 59), (10, 0), (10, 8),
         (10, 9), (11, 59), (12, 0), (12, 1), (12, 59), (13, 0), (23, 58), (23, 59))


def run(args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, timeout=120, **kwargs)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def provenance():
    record = json.loads((ASSETS / 'SOURCES.json').read_text())
    for path in ('src/util/DeskClockFaces.h', 'src/util/DeskClockDigits.h', 'scripts/generate_clock_digits.py', 'LICENSE'):
        assert sha((FIXTURE / Path(path).name).read_bytes()) == record['inputs'][path]['sha256']
    for family in ('NotoSans', 'NotoSerif'):
        original = f'lib/EpdFont/builtinFonts/source/{family}/OFL.txt'
        assert sha((ASSETS / f'LICENSE-{family}.txt').read_bytes()) == record['inputs'][original]['sha256']
    for name, asset in record['assets'].items():
        assert sha((ASSETS / name).read_bytes()) == asset['sha256'], name
    run(['python3', ROOT / 'scripts/generate_desk_clock_assets.py', '--check'])


def compile_host(folder, sanitized):
    cc, cxx = os.environ.get('CC', 'cc'), os.environ.get('CXX', 'c++')
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    common = ['-Wall', '-Wextra', '-Werror', '-I' + str(INCLUDE), *flags]
    obj, exe = folder / 'faces.o', folder / 'test'
    run([cc, '-std=c11', '-pedantic', *common, '-c', ROOT / 'lib/PortableApps/src/desk_clock_faces.c', '-o', obj])
    # Existing C policy uses {0} aggregate initializers; suppress this C++-only warning.
    run([cxx, '-std=c++17', '-Wno-missing-field-initializers', *common,
         ROOT / 'test/native_apps/portable_desk_faces_test.cpp', obj, '-o', exe])
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    run([exe], env=env)
    return exe


def rows(pbm, width, height):
    header = f'P4\n{width} {height}\n'.encode()
    assert pbm.startswith(header) and len(pbm) == len(header) + width * height // 8
    pixels = pbm[len(header):]
    return [int.from_bytes(pixels[y * width // 8:(y+1) * width // 8], 'big') for y in range(height)]


def near(a, b):
    # Every changed edge remains within one pixel of an edge in the other raster.
    for y, row in enumerate(a):
        other = b[y] | (b[y-1] if y else 0) | (b[y+1] if y+1 < len(b) else 0)
        assert not row & ~(other | (other << 1) | (other >> 1)), 'Analog geometry moved more than one pixel'


def raster_compare(exe, write_evidence):
    result = []
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    historical = {entry['case']: entry for entry in json.loads((HISTORICAL_EVIDENCE / 'rasters.json').read_text())}
    historical_captures = {p.name: sha(p.read_bytes()) for p in sorted(HISTORICAL_EVIDENCE.glob('*.png'))}
    assert json.loads((HISTORICAL_EVIDENCE / 'capture-sha256.json').read_text()) == historical_captures, 'Historical PNG evidence changed'
    adjusted_cases = 0
    if write_evidence:
        from PIL import Image, ImageDraw
        EVIDENCE.mkdir(parents=True, exist_ok=True)
    for width, height in ((480, 800), (800, 480)):
        for face, name in enumerate(FACES):
            for hour, minute, fmt, valid in [(h, m, f, 1) for h, m in TIMES for f in (0, 1)] + [(0, 0, 0, 0)]:
                key = f'{width}x{height}-{name.lower()}-{hour:02}{minute:02}-{12 if fmt else 24}h' if valid else f'{width}x{height}-{name.lower()}-invalid'
                args = [exe, 'render', face, width, height, hour, minute, fmt, valid]
                port = subprocess.check_output(list(map(str, [*args, 0])), timeout=10, env=env)
                reader = subprocess.check_output(list(map(str, [*args, 1])), timeout=10, env=env)
                reference = subprocess.check_output(list(map(str, [*args, 2])), timeout=10, env=env)
                hour_displayed = (hour % 12 or 12) if fmt else hour
                adjusted = bool(valid and hour_displayed < 10 and (face == 0 or (face < 3 and not fmt)))
                if adjusted:
                    # Explicit feature boundary, never a tolerance/skip: the new
                    # reference must differ from Reader and match current pixels.
                    assert reference != reader, ('Unpadded negative control did not differ', key)
                    adjusted_cases += 1
                else:
                    assert reference == reader, ('Reference changed outside unpadded layout', key)
                    assert sha(port) == historical[key]['portable_pbm_sha256'], ('Unchanged historical raster moved', key)
                if face < 3 or not valid:
                    assert sha(reader) == historical[key]['reader_pbm_sha256'], ('Frozen Reader digital raster changed', key)
                a, b = rows(port, width, height), rows(reference, width, height)
                changed = sum((x ^ y).bit_count() for x, y in zip(a, b))
                if face < 3 or not valid:
                    assert port == reference, key
                else:
                    assert changed <= 512, (key, changed)
                    near(a, b); near(b, a)
                historical_changed = sum((x ^ y).bit_count() for x, y in zip(a, rows(reader, width, height)))
                entry = dict(case=key, portable_pbm_sha256=sha(port), reference_pbm_sha256=sha(reference),
                             reader_pbm_sha256=sha(reader), changed_pixels=changed,
                             unpadded_layout=adjusted, historical_changed_pixels=historical_changed)
                result.append(entry)
                # Native-size captures include one-digit layouts in both formats,
                # the original representative times, and invalid time.
                if write_evidence and ((hour, minute, fmt) in ((0, 0, 0), (9, 5, 0), (9, 5, 1), (10, 8, 0), (23, 59, 1)) or not valid):
                    for label, data in (('portable', port), ('reference', reference), ('reader', reader)):
                        Image.open(io.BytesIO(data)).save(EVIDENCE / f'{key}-{label}.png', optimize=True)
    assert len(result) == len(historical) == 396
    assert adjusted_cases == 44, ('Unpadded feature coverage changed', adjusted_cases)
    golden = EVIDENCE / 'rasters.json'
    if write_evidence:
        golden.write_text(json.dumps(result, indent=2) + '\n')
        for width, height in ((480, 800), (800, 480)):
            for time_key, caption, prefix in (('0000-24h', '0:00 / 24h', 'comparison-midnight'),
                                             ('0905-24h', '9:05 / 24h', 'comparison-one-digit'),
                                             ('0905-12h', '9:05 / 12h', 'comparison-one-digit-12h'),
                                             ('1008-24h', '10:08 / 24h', 'comparison'),
                                             ('2359-12h', '23:59 / 12h (11:59)', 'comparison-late')):
                tile_w, tile_h = width // 2, height // 2
                sheet = Image.new('RGB', (3 * (2 * tile_w + 16) + 16, 2 * (tile_h + 52) + 16), 'white')
                draw = ImageDraw.Draw(sheet)
                for face, name in enumerate(FACES):
                    x, y = 16 + (face % 3) * (2 * tile_w + 16), 16 + (face // 3) * (tile_h + 52)
                    draw.text((x, y), f'{face}: {name}   {caption}', fill='black')
                    for j, label in enumerate(('reference', 'portable')):
                        draw.text((x + j * tile_w, y + 16), label, fill='black')
                        img = Image.open(EVIDENCE / f'{width}x{height}-{name.lower()}-{time_key}-{label}.png')
                        sheet.paste(img.resize((tile_w, tile_h), Image.Resampling.NEAREST), (x + j * tile_w, y + 32))
                sheet.save(EVIDENCE / f'{prefix}-{width}x{height}.png', optimize=True)
        nova = ROOT / 'docs/nova/screens/paper-clock.png'
        sheet = Image.new('RGB', (3 * 480, 850), 'white'); draw = ImageDraw.Draw(sheet)
        for x, title, path in ((0, 'Existing Nova7 main clock (unchanged)', nova),
            (480, 'Portable Sans / Reader numeral spans', EVIDENCE / '480x800-sans-1008-24h-portable.png'),
            (960, 'Portable Railway / Reader geometry', EVIDENCE / '480x800-railway-1008-24h-portable.png')):
            draw.text((x + 12, 12), title, fill='black'); sheet.paste(Image.open(path).convert('RGB'), (x, 40))
        sheet.save(EVIDENCE / 'nova7-context.png', optimize=True)
    else:
        # Reader uses host libm; its analog rounding may vary by libm build.
        # The portable integer raster is the strict cross-platform golden.
        expected = json.loads(golden.read_text())
        identity = lambda cases: [(c['case'], c['portable_pbm_sha256']) for c in cases]
        assert identity(expected) == identity(result), 'Portable raster golden changed; review pixels before refreshing evidence'
    capture_hashes = {p.name: sha(p.read_bytes()) for p in sorted(EVIDENCE.glob('*.png'))}
    if write_evidence:
        (EVIDENCE / 'capture-sha256.json').write_text(json.dumps(capture_hashes, indent=2) + '\n')
    else:
        assert json.loads((EVIDENCE / 'capture-sha256.json').read_text()) == capture_hashes, 'PNG evidence changed'
    maxima = {name: max(e['changed_pixels'] for e in result if '-' + name.lower() + '-' in e['case']) for name in FACES}
    print(f'{len(result)} current pixel comparisons and golden hashes pass; {adjusted_cases} explicit unpadded layouts, '
          f'{len(result) - adjusted_cases} unchanged historical goldens; maximum changed pixels: {maxima}')
    return {'cases': len(result), 'unpadded_layout_cases': adjusted_cases,
            'unchanged_historical_cases': len(result) - adjusted_cases,
            'maximum_changed_pixels': maxima, 'golden_sha256': sha(golden.read_bytes()),
            'historical_golden_sha256': sha((HISTORICAL_EVIDENCE / 'rasters.json').read_bytes())}


def target(folder, compiler):
    cc = str(compiler)
    mapping = folder / 'exports.map'; mapping.write_text('{ global: app_main; local: *; };\n')
    fixture = folder / 'link.c'
    fixture.write_text('#include "PortableDeskClockFaces.h"\n'
        'static volatile unsigned count;\n'
        'static bool fill(void *p,int x,int y,int w,int h,bool b){(void)p;count+=(unsigned)(x+y+w+h+b);return true;}\n'
        '__attribute__((visibility("default"))) void app_main(void){portable_desk_canvas c={0,480,800,fill};\n'
        '(void)portable_desk_draw_face(&c,count%6,10,8,false,true);}\n')
    elf = folder / 'desk-clock-faces.elf'
    run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls', '-fvisibility=hidden',
         '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared', '-Wl,--hash-style=sysv',
         '-Wl,--no-undefined', '-Wl,--version-script=' + str(mapping), '-Wall', '-Wextra', '-Werror',
         '-I' + str(INCLUDE), ROOT / 'lib/PortableApps/src/desk_clock_faces.c', fixture, '-o', elf])
    # Deliberately no -lm / -lgcc: no Runtime symbols or compiler helper imports.
    symbols = subprocess.check_output([cc.removesuffix('gcc') + 'nm', '-D', str(elf)], text=True)
    imports = [s.split()[-1] for s in symbols.splitlines() if ' U ' in ' ' + s]
    exports = [s.split()[-1] for s in symbols.splitlines() if len(s.split()) >= 3 and s.split()[-2] in ('T','D','B','R')]
    assert imports == [] and exports == ['app_main'], (imports, exports)
    validator = folder / 'validate'
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I' + str(ROOT / 'test/native_apps/stubs'), '-I' + str(ROOT / 'lib/elf_loader/include'),
         ROOT / 'lib/elf_loader/src/esp_elf_validate.c', ROOT / 'test/native_apps/validate_test.c', '-o', validator])
    run([validator, elf])
    print('Xtensa face-only witness: structural validator passes; zero imports, no libgcc/libm, only app_main exported')
    return {'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
            'elf_sha256': sha(elf.read_bytes()), 'elf_bytes': elf.stat().st_size,
            'imports': imports, 'exports': exports, 'structural_validation': 'pass'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write-evidence', action='store_true', help='Replace current unpadded evidence; preserve historical Reader evidence')
    parser.add_argument('--target-cc', type=Path, help='Also build/validate Xtensa face-only ELF with this compiler')
    args = parser.parse_args()
    provenance()
    with tempfile.TemporaryDirectory(prefix='desk-faces-') as tmp:
        folder = Path(tmp)
        exe = compile_host(folder, False)
        raster = raster_compare(exe, args.write_evidence)
        sanitized_exe = compile_host(folder, True)
        sanitized_raster = raster_compare(sanitized_exe, False)
        assert sanitized_raster == raster, 'Normal/sanitized raster evidence differs'
        validation = {'raster': raster, 'normal_and_asan_ubsan': 'pass',
                      'sources': {str(path.relative_to(ROOT)): sha(path.read_bytes()) for path in (
                          ROOT / 'lib/PortableApps/src/desk_clock_faces.c',
                          FIXTURE / 'DeskClockFaces.h', FIXTURE / 'UnpaddedLayout.h',
                          ROOT / 'test/native_apps/portable_desk_faces_test.cpp', Path(__file__).resolve())}}
        if args.target_cc:
            validation['target'] = target(folder, args.target_cc)
        if args.write_evidence:
            (EVIDENCE / 'validation.json').write_text(json.dumps(validation, indent=2) + '\n')


if __name__ == '__main__':
    main()
