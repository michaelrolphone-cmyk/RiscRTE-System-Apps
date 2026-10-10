#!/usr/bin/env python3
"""Prove the current raster gate rejects historical-format regressions."""
import argparse
import json
import os
from pathlib import Path
import tempfile

import test_desk_clock_faces as faces


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, help='Save the source-bound mutation results')
    args = parser.parse_args()
    source_path = faces.ROOT / 'lib/PortableApps/src/desk_clock_faces.c'
    original = source_path.read_text()
    controls = (
        ('restore-padded-segments-24h', 'bool short_hour=valid && hour<10;',
         'bool short_hour=valid && use12_hour && hour<10;', '480x800-segments-0000-24h'),
        ('restore-four-digit-segments-spacing', 'int columns=short_hour?22:29, shift=short_hour?7:0;',
         'int columns=29, shift=0;', '480x800-segments-0000-24h'),
        ('restore-padded-noto-24h', 'int first = hour < 10 ? 1 : 0',
         'int first = use12_hour && hour < 10 ? 1 : 0', '480x800-sans-0000-24h'),
    )
    results = []
    with tempfile.TemporaryDirectory(prefix='desk-layout-controls-') as temporary:
        folder = Path(temporary)
        for name, before, after, expected in controls:
            assert original.count(before) == 1, ('Mutation source no longer matches', name)
            # Change only a temporary copy. No production or fixture file is edited.
            source, obj, exe = folder / 'faces.c', folder / 'faces.o', folder / 'test'
            source.write_text(original.replace(before, after))
            common = ['-Wall', '-Wextra', '-Werror', '-I' + str(faces.INCLUDE)]
            faces.run([os.environ.get('CC', 'cc'), '-std=c11', '-pedantic', *common,
                       '-I' + str(source_path.parent), '-c', source, '-o', obj])
            faces.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wno-missing-field-initializers',
                       *common, faces.ROOT / 'test/native_apps/portable_desk_faces_test.cpp', obj, '-o', exe])
            try:
                faces.raster_compare(exe, False)
            except AssertionError as error:
                assert str(error) == expected, (name, str(error))
            else:
                raise AssertionError('Mutation incorrectly passed: ' + name)
            results.append({'mutation': name, 'rejected_at': expected,
                            'mutated_source_sha256': faces.sha(source.read_bytes())})
            print(f'PASS negative control: {name} rejected at {expected}')
    assert source_path.read_text() == original, 'Production source changed'
    if args.receipt:
        sources = (source_path, Path(faces.__file__).resolve(), Path(__file__).resolve(),
                   faces.FIXTURE / 'DeskClockFaces.h', faces.FIXTURE / 'UnpaddedLayout.h',
                   faces.ROOT / 'test/native_apps/portable_desk_faces_test.cpp')
        receipt = {'purpose': 'Historical-format mutations must fail the current raster gate; production is unchanged.',
                   'sources': {str(path.relative_to(faces.ROOT)): faces.sha(path.read_bytes()) for path in sources},
                   'controls': results}
        args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    main()
