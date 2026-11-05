#!/usr/bin/env python3
"""Production paper Quick complete-label, native-height and chrome regression.

Reuses the coordinate provider's input, delayed-present immutability and exact
restoration assertions. The independent raster oracle reads licensed glyph
metrics/bits and frozen reference observation slots, not pp_text or qp_text.
Builds are sequential; binary hashes are retained and executables removed.
Raw PBM captures are losslessly normalized to retained PNGs to bound disk use.
"""
import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'test/native_apps'
FONT_PATH = ROOT / 'lib/PortableApps/paper_fonts/text.inc'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_glyphs():
    source = FONT_PATH.read_text()
    bitmaps = {name: bytes(map(int, values.split(','))) for name, values in
               re.findall(r'static const uint8_t (rpp_\d+_\d+)\[\] = \{([\d,]+)\};', source)}
    records = re.findall(r'\{(\d+),(\d+),(-?\d+),(-?\d+),(\d+),(rpp_\d+_\d+)\}', source)
    assert len(records) == 190
    return [(*map(int, r[:5]), bitmaps[r[5]]) for r in records]


GLYPHS = load_glyphs()
SLOTS = json.loads((FIXTURES / 'paper_label_slots.json').read_text())['slots']


def glyphs(text, heading):
    return [GLYPHS[(0 if heading else 95) + ord(c.upper()) - 32] for c in text]


def measure(text, heading):
    return (sum(g[4] for g in glyphs(text, heading)) + 15) // 16


def font_mask(text, heading, available=None):
    """Transform complete source black runs horizontally; never trim strings.

    Q4 advances and source bitmaps define the independent oracle. Each source
    run's two endpoints are projected separately; native source rows stay fixed.
    Include every character, including the final glyph's final black run.
    """
    gs = glyphs(text, heading)
    total = measure(text, heading)
    fitted = min(total, available) if available is not None else total
    mask = Image.new('1', (max(total, available or total) + 4, 40), 255)
    pixels = mask.load()
    pen = 0
    character_bounds = []
    for width, height, left, top, advance, bits in gs:
        painted = []
        for row in range(height):
            col = 0
            while col < width:
                if not (bits[(row * width + col) // 8] & (128 >> ((row * width + col) % 8))):
                    col += 1
                    continue
                start = col
                col += 1
                while col < width and bits[(row * width + col) // 8] & (128 >> ((row * width + col) % 8)):
                    col += 1
                first = (pen // 16 + left + start) * fitted // total
                last = (pen // 16 + left + col) * fitted // total
                for x in range(first, last):
                    assert x >= 0
                    pixels[x, top + row] = 0
                    painted.append((x, top + row))
        character_bounds.append([min(x for x, _ in painted), min(y for _, y in painted),
                                 max(x for x, _ in painted) + 1, max(y for _, y in painted) + 1]
                                if painted else None)
        pen += advance
    return mask, total, fitted, character_bounds


def profile_slots(profile):
    substitutions = {'$time': profile['time'], '$notifications': 'NOTIFICATIONS' if profile['sound'] else 'SOUND UNAVAILABLE',
                     '$volume': '50%' if profile['sound'] else '--%'}
    for slot in SLOTS:
        if slot.get('no_sound_only') and profile['sound']:
            continue
        x, y, width = slot['rect']
        if not profile['sound']:
            y = slot.get('no_sound_y', y)
        yield slot['id'], x, y, width, substitutions.get(slot['text'], slot['text']), slot.get('heading', False)


def make_profiles():
    def widest(hours):
        labels = [f'{hour:02}:{minute:02}' for hour in hours for minute in range(60)]
        label = max(labels, key=lambda s: (measure(s, True), s))
        return label, measure(label, True)
    wide24, width24 = widest(range(24))
    wide12, width12 = widest(range(1, 13))
    profiles = [dict(id='default24', time='20:59', hour=20, minute=59, format=1, known=1, sound=1),
                dict(id='widest24', time=wide24, hour=int(wide24[:2]), minute=int(wide24[3:]), format=1, known=1, sound=1),
                dict(id='widest12', time=wide12, hour=int(wide12[:2]), minute=int(wide12[3:]), format=0, known=1, sound=1),
                dict(id='unknown', time='--:--', hour=20, minute=59, format=1, known=0, sound=1),
                dict(id='no-sound', time='20:59', hour=20, minute=59, format=1, known=1, sound=0)]
    return profiles, {'all_24_hour_labels': 1440, 'all_12_hour_labels': 720,
                      'widest_24_hour': wide24, 'widest_24_width': width24,
                      'widest_12_hour': wide12, 'widest_12_width': width12}


def black_bounds(im):
    return ImageChops.invert(im.convert('L')).getbbox()


def verify_labels(im, width, height, profile, selected=None):
    results = []
    for ident, x, y, slot_width, text, heading in profile_slots(profile):
        if selected is not None and ident not in selected:
            continue
        px, py, available = x * width // 480, y * height // 800, slot_width * width // 480
        mask, total, fitted, characters = font_mask(text, heading, available)
        # Full native glyph-height box with a white one-pixel guard on all sides.
        # Observe font bounds rather than extending into the next control row.
        bounds = black_bounds(mask)
        top, bottom = bounds[1], bounds[3]
        expected = Image.new('1', (available + 2, bottom - top + 2), 255)
        expected.paste(mask.crop((0, top, available, bottom)), (1, 1))
        actual = im.crop((px - 1, py + top - 1, px + available + 1, py + bottom + 1))
        assert actual.tobytes() == expected.tobytes(), f'{profile["id"]} {width}x{height} {ident}: incomplete/different label {text!r}'
        assert characters[-1] is not None, (ident, 'final glyph absent')
        bounds = black_bounds(mask)
        assert bounds and bounds[2] <= fitted <= available
        assert bounds[3] <= 30
        natural, _, _, _ = font_mask(text, heading)
        assert bounds[1] == black_bounds(natural)[1] and bounds[3] == black_bounds(natural)[3], (ident, 'native glyph height changed')
        results.append(dict(id=ident, text=text, origin=[px, py], available_width=available,
                            advance=total, fitted_width=fitted, bounds=list(bounds),
                            character_bounds=characters, complete_mask_sha256=hashlib.sha256(mask.tobytes()).hexdigest(),
                            native_height_preserved=True, complete_mask_equal=True, neighboring_guard_equal=True))
    return results


def chrome_oracle(reference):
    """Discover solid/hollow rectangular components in the frozen 480 raster.

    No Quick control layout is reproduced. Component shape and one-pixel white
    guard ring establish chrome independently of the production drawing code.
    """
    width, height = reference.size
    pix = reference.load()
    unseen = {(x, y) for y in range(height) for x in range(width) if not pix[x, y]}
    rectangles = []
    while unseen:
        first = unseen.pop()
        component, stack = {first}, [first]
        while stack:
            x, y = stack.pop()
            for point in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if point in unseen:
                    unseen.remove(point); component.add(point); stack.append(point)
        if len(component) < 60:
            continue
        left, right = min(x for x, _ in component), max(x for x, _ in component) + 1
        top, bottom = min(y for _, y in component), max(y for _, y in component) + 1
        w, h = right - left, bottom - top
        if w < 18:  # Exclude solid vertical heading-glyph components.
            continue
        for border in range(1, min(w, h, 8) + 1):
            expected = {(x, y) for y in range(top, bottom) for x in range(left, right)
                        if x < left + border or x >= right - border or y < top + border or y >= bottom - border}
            if expected == component:
                rectangles.append(dict(rect=[left, top, right, bottom], border=border))
                break
    assert len(rectangles) >= 30, len(rectangles)
    samples = {}
    for item in rectangles:
        left, top, right, bottom = item['rect']; border = item['border']
        for y in range(max(0, top - 1), min(height, bottom + 1)):
            for x in range(max(0, left - 1), min(width, right + 1)):
                if x <= left + border or x >= right - border - 1 or y <= top + border or y >= bottom - border - 1:
                    samples[(x, y)] = bool(pix[x, y])
    return rectangles, samples


def check_chrome(im, samples):
    width, height = im.size
    pix = im.load(); count = 0
    for (x, y), color in samples.items():
        x0, x1 = (x * width + 479) // 480, ((x + 1) * width + 479) // 480
        y0, y1 = (y * height + 799) // 800, ((y + 1) * height + 799) // 800
        for yy in range(y0, y1):
            for xx in range(x0, x1):
                assert bool(pix[xx, yy]) == color, f'chrome changed at ({xx},{yy}) from frozen ({x},{y})'
                count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-sdk', type=Path, required=True)
    parser.add_argument('--display-sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/paper-label-fit')
    parser.add_argument('--configs', help='Optional comma-separated configuration IDs, for focused local checks')
    args = parser.parse_args(); out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    profiles, exhaustive = make_profiles()
    baseline_dir = ROOT / 'docs/paper-coordinate-geometry'
    baseline_meta = json.loads((baseline_dir / 'baseline.json').read_text())
    baseline_path = baseline_dir / 'legacy-480x800.png'
    assert digest(baseline_path) == baseline_meta['png_sha256'][baseline_path.name]
    reference = Image.open(baseline_path).convert('1')
    reference_labels = verify_labels(reference, 480, 800, profiles[0])
    chrome_rectangles, chrome_samples = chrome_oracle(reference)
    negative_path = baseline_dir / 'fixed-410x700.png'
    negative = Image.open(negative_path).convert('1')
    negative_controls = {}
    for ident in ['time', 'airplane-status', 'wifi-status', 'bluetooth-status', 'torch-status']:
        try:
            verify_labels(negative, 410, 700, profiles[0], {ident})
        except AssertionError as error:
            negative_controls[ident] = dict(rejected=True, reason=str(error))
        else:
            raise AssertionError('Oracle did not reject known pre-label clipping: '+ident)
    clock_meta = json.loads((FIXTURES / 'paper_label_clock_baseline.json').read_text())
    for value in clock_meta['captures'].values():
        assert digest(ROOT / value['capture']) == value['capture_sha256']
    include = out / 'include'
    shutil.copytree(ROOT / 'lib/PortableApps/include', include, dirs_exist_ok=True)
    shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
    sdk = []
    for folder, names in [(args.runtime_sdk, ['RiscRuntimeV1.h','RiscRuntimeHostControlV1.h','RiscResidentShellV1.h','RiscFailureEvidenceV1.h']),
                          (args.display_sdk, ['RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h'])]:
        for name in names:
            source = (folder / name).resolve(); staged = include / name
            shutil.copyfile(source, staged)
            assert digest(source) == digest(staged)
            sdk.append(dict(source=str(source), staged=str(staged), sha256=digest(staged)))
    common = ['-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_OWNS_TOUCH_CHROME',
              '-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_CROWN_SLEEP_UNAVAILABLE']
    sanitized = ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    configs = [(f'width{w}', w, 600 if w == 400 else 800 if w == 480 else 1000 if w == 600 else 700, [])
               for w in [410,400,403,404,419,420,428,429,432,433,480,600]]
    configs += [(f'snapshot{w}', w, 600 if w == 400 else 1000 if w == 600 else 700, ['-DPORTABLE_RASTER_SNAPSHOT']) for w in [400,410,600]]
    configs += [('rotate410',410,700,['-DPORTABLE_DISPLAY_ROTATION=90']),
                ('flip410',410,700,['-DPORTABLE_PAPER_PREFERENCES','-DTEST_READER_FLIP']),
                ('rawrotate410',410,700,['-DPORTABLE_TOUCH_ROTATION=180','-DTEST_RAW_TOUCH_ROTATION']),
                ('sanitized410',410,700,sanitized)]
    if args.configs:
        selected = set(args.configs.split(',')); assert selected <= {c[0] for c in configs}
        configs = [c for c in configs if c[0] in selected]
    sources = [ROOT / 'Apps/paper_clock.c', FIXTURES / 'paper_coordinate_test.c', FIXTURES / 'paper_label_fit_fixture.c',
               ROOT / 'lib/PortableApps/src/adapter.c'] + [ROOT / 'lib/PortableApps/src' / n for n in ['quick_actions.c','quick_render.c','quick_session.c']]
    source_inputs = [Path(__file__).resolve(), FIXTURES / 'paper_label_slots.json', FIXTURES / 'paper_label_clock_baseline.json',
                     ROOT / '.github/workflows/paper-label-fit.yml', *sources]
    source_inputs += sorted(FIXTURES.glob('paper_label_clock_*.png'))
    for source in source_inputs:
        assert source.is_file(), 'Required receipt source is missing: '+str(source)
    source_hashes = {str(p.relative_to(ROOT)): digest(p) for p in source_inputs}
    receipt = dict(source_head=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
                   source_tree=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD^{tree}'],text=True).strip(),
                   hardware_run=False, source_inputs_sha256=source_hashes, sdk_staged_provenance=sdk,
                   working_tree_status=subprocess.check_output(['git','-C',str(ROOT),'status','--short'],text=True),
                   tracked_diff_sha256=hashlib.sha256(subprocess.check_output(['git','-C',str(ROOT),'diff','--binary'])).hexdigest(),
                   frozen_quick_baseline_sha256=digest(baseline_path), non_quick_clock_baseline=clock_meta,
                   negative_pre_label_410_control=dict(path=str(negative_path),sha256=digest(negative_path),comparisons=negative_controls),
                   font_sha256=digest(FONT_PATH), exhaustive_time_metrics=exhaustive, profiles=profiles,
                   frozen_reference_label_comparisons=reference_labels, chrome_rectangles=chrome_rectangles,
                   results=[], source_closure=[], executable_sha256={}, comparisons={})
    with (out / 'test.log').open('w') as log:
        try:
            for name, width, height, flags in configs:
                physical_width, physical_height = (height, width) if name.startswith('rotate') else (width, height)
                dimensions = [f'-DTEST_PAPER_WIDTH={physical_width}',f'-DTEST_PAPER_HEIGHT={physical_height}',
                              f'-DTEST_PAPER_LOGICAL_WIDTH={width}',f'-DTEST_PAPER_LOGICAL_HEIGHT={height}']
                binary = out / (name + '.bin')
                command = ['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(include),
                           '-I'+str(ROOT / 'lib/NativeApps/include'), *common, *dimensions, *flags, *map(str,sources)]
                dependencies = subprocess.run(command + ['-MM'], check=True, capture_output=True, text=True).stdout
                (out / (name + '.dependencies.mk')).write_text(dependencies)
                dependency_paths = sorted({str(Path(token).resolve()) for token in shlex.split(dependencies.replace('\\\n',' ')) if not token.endswith(':')})
                closure = {p:digest(p) for p in dependency_paths}
                build_command = command + ['-no-pie','-Wl,--wrap=risc_runtime_get_api','-o',str(binary)]
                receipt['source_closure'].append(dict(configuration=name,command=build_command,sha256=closure))
                log.write('BUILD ' + shlex.join(build_command) + '\n'); log.flush()
                try:
                    subprocess.run(build_command,check=True,stdout=log,stderr=log)
                    cases = [(profile,0) for profile in profiles] + [(profiles[0],1),(profiles[0],2)]
                    for profile, mode in cases:
                        ident = f'{name}-{profile["id"]}-mode{mode}'
                        capture = out / ident; capture.mkdir(exist_ok=True)
                        writes = 0 if mode == 1 or not profile['sound'] else 2
                        cmd = [str(binary),str(130*width//480),str(345*height//800),str(writes),str(capture),str(mode)]
                        env = dict(os.environ,ASAN_OPTIONS='detect_leaks=0', **{'PAPER_LABEL_'+key.upper():str(profile[key]) for key in ['hour','minute','format','known','sound']})
                        result = subprocess.run(cmd,timeout=30,capture_output=True,text=True,env=env)
                        log.write('RUN '+shlex.join(cmd)+'\n'+result.stdout+result.stderr); log.flush()
                        case = dict(id=ident, configuration=name, profile=profile['id'], probe_mode=mode,
                                    command=cmd, provider_environment={k:v for k,v in env.items() if k.startswith('PAPER_LABEL_')},
                                    returncode=result.returncode,stdout=result.stdout,stderr=result.stderr,captures=[])
                        receipt['results'].append(case)
                        result.check_returncode()
                        for pbm in sorted(capture.glob('frame-*.pbm')):
                            raw_hash = digest(pbm); im = Image.open(pbm).convert('1')
                            if name.startswith('rotate'): im = im.rotate(270,expand=True)
                            if name.startswith('flip'): im = im.rotate(180)
                            png = pbm.with_suffix('.png'); im.save(png)
                            case['captures'].append(dict(path=str(png),sha256=digest(png),raw_provider_sha256=raw_hash,size=list(im.size)))
                            pbm.unlink()
                        frame = Image.open(capture / 'frame-2.png').convert('1')
                        assert frame.size == (width,height)
                        case['labels'] = verify_labels(frame,width,height,profile)
                        case['chrome_pixels_compared'] = check_chrome(frame,chrome_samples)
                        first = Image.open(capture / 'frame-1.png').convert('1')
                        last = Image.open(sorted(capture.glob('frame-*.png'), key=lambda p:int(p.stem.split('-')[-1]))[-1]).convert('1')
                        assert first.tobytes() == last.tobytes(), ident+' exact restore'
                        receipt['comparisons'][ident+'-exact-restore'] = True
                        if profile['id']=='default24' and width in [410,480]:
                            frozen = Image.open(ROOT / clock_meta['captures'][str(width)]['capture']).convert('1')
                            assert first.size == frozen.size and first.tobytes() == frozen.tobytes(), ident+' non-Quick Clock changed'
                            receipt['comparisons'][ident+'-non-quick-clock-unchanged'] = True
                        if width==480 and profile['id']=='default24':
                            assert frame.tobytes()==reference.tobytes(), ident+' frozen 480 Quick changed'
                            receipt['comparisons'][ident+'-frozen-480-quick-unchanged'] = True
                        print(ident+' PASS',flush=True)
                finally:
                    if binary.exists():
                        receipt['executable_sha256'][str(binary)] = digest(binary); binary.unlink()
            # Byte-exact normalized modal output across all timing/rotation modes.
            for case in receipt['results']:
                direct = out / f'width{next(c[1] for c in configs if c[0]==case["configuration"])}-{case["profile"]}-mode0/frame-2.png'
                actual = out / case['id'] / 'frame-2.png'
                if direct.is_file():
                    a,b=Image.open(actual).convert('1'),Image.open(direct).convert('1')
                    assert a.size==b.size and a.tobytes()==b.tobytes(),case['id']+' normalized differs'
                    receipt['comparisons'][case['id']+'-normalized-direct-equal']=True
            receipt['status'] = 'PASS'
        except BaseException as exc:
            receipt['status']='FAIL';receipt['failure']=repr(exc)
            raise
        finally:
            receipt['source_inputs_unchanged'] = all(digest(ROOT/p)==h for p,h in source_hashes.items())
            receipt['compiler_dependencies_unchanged'] = all(digest(p)==h for c in receipt['source_closure'] for p,h in c['sha256'].items())
            (out / 'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'{len(receipt["results"])} production adapter/provider cases, '
          f'{sum(len(r.get("labels",[])) for r in receipt["results"])} complete label masks, '
          f'{len(receipt["comparisons"])} full-frame comparisons PASS; no executables retained')


if __name__ == '__main__':
    main()
