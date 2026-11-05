#!/usr/bin/env python3
"""Offline publisher volume policy and exact store-byte tests; no native admission.

Use the generated JSON with test_installed_apps_boot_profile.sh for the actual
service parser. The synthetic SPIFFS image and ELF bytes are never a product.
"""
import argparse, copy, hashlib, json, struct, tempfile
from pathlib import Path
import build_boot_profile_feed as feed

BASE = 'https://example.invalid/immutable-fixture/'

def image_fixture(files, size):
    image = bytearray(b'\xff' * size)
    for block in range(size // 4096):
        struct.pack_into('<H', image, block * 4096 + 252,
                         (0x20140529 ^ 256 ^ (size // 4096 - block)) & 0xffff)
    used = 0
    def page(object_id, span, index):
        nonlocal used
        block, entry = divmod(used, 15); used += 1
        number = block * 16 + entry + 1; offset = number * 256
        struct.pack_into('<H', image, block * 4096 + entry * 2, object_id)
        struct.pack_into('<HHB', image, offset, object_id, span, 0xf8 if index else 0xfc)
        return number, offset
    for identity, (name, data) in enumerate(sorted(files.items()), 1):
        assert len(name.encode()) < 31 and 0 < len(data) <= 103 * 251
        _, header = page(identity | 0x8000, 0, True)
        struct.pack_into('<IB', image, header + 8, len(data), 1)
        name = b'/' + name.encode() + b'\0'
        image[header + 13:header + 13 + len(name)] = name
        for span, offset in enumerate(range(0, len(data), 251)):
            number, at = page(identity, span, False)
            struct.pack_into('<H', image, header + 49 + span * 2, number)
            chunk = data[offset:offset + 251]
            image[at + 5:at + 5 + len(chunk)] = chunk
    return bytes(image)

def fixture(p, volume):
    firmware = b'fixture-only\0RISC_RUNTIME_VERSION:0.2.24\0RISC_PAIRED_STORE_ABI:3\0'
    blobs = {'default.elf': b'fixture-home-image', 'sd.elf': b'fixture-volume-image',
             'aux.elf': b'fixture-driver-image'}
    manifests = {
        'default.json': {'type': 'application', 'id': 'home', 'file_name': 'default.elf'},
        'sd.json': {'type': 'driver', 'id': 'volume', 'file_name': 'sd.elf',
                    'provides': [{'capability': 'storage.volume', 'api': 1}]},
        'aux.json': {'type': 'driver', 'id': 'aux', 'file_name': 'aux.elf'}}
    def mirror(filename, package, driver=False):
        return {'volume_instance': volume, 'internal_fallback': True,
                'path': ('Drivers/' if driver else 'Apps/') + package + '/image-' + p.sha(blobs[filename]) + '.elf'}
    boot = {'board': 'board.json', 'default_app': 'default.elf',
            'drivers': [{'manifest': 'sd.json', 'instance_id': volume},
                        {'manifest': 'aux.json', 'instance_id': 20, 'image_source': mirror('aux.elf', 'aux', True)}],
            'app_capabilities': [{'manifest': 'default.json', 'image_source': mirror('default.elf', 'home'),
                'grants': [{'capability': 'storage.volume', 'api': 1, 'instance_id': volume},
                           {'capability': 'storage.key-value', 'api': 1, 'instance_id': 6}]}]}
    cohort = {'schema': 'riscrte.cohort', 'schema_version': 1, 'product': 'xteink-x4-pro',
              'version': '0.1.123', 'source_repo': 'fixture/product', 'source_revision': '0' * 40,
              'layout': 'riscrte-paired-appdata-v3', 'runtime_version': '0.2.24', 'store_abi': 3,
              'firmware_size': len(firmware), 'firmware_sha256': p.sha(firmware)}
    files = {**blobs, **{name: p.encode(value) for name, value in manifests.items()},
             'boot.json': p.encode(boot), 'board.json': b'{}', 'cohort.json': p.encode(cohort)}
    return files, firmware

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); args.runtime = args.runtime.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    helpers = feed.load_runtime(args.runtime); p = helpers.profile; runs = []
    for volume in (9, 12):
        files, firmware = fixture(p, volume)
        _, records = feed.store_closure(p, files)
        before = copy.deepcopy((files, records))
        with tempfile.TemporaryDirectory(prefix='boot-feed-volume-') as directory:
            root = Path(directory); store = root / 'store'; store.mkdir()
            for name, data in files.items(): (store / name).write_bytes(data)
            image = image_fixture(files, p.LAYOUTS['riscrte-paired-appdata-v3'])
            assert helpers.read_image(image, len(image)) == files
            (root / 'image.bin').write_bytes(image); (root / 'firmware.bin').write_bytes(firmware)
            def build(name, selected):
                return feed.build(args.runtime, store, root / 'image.bin', root / 'firmware.bin',
                    BASE, 'xteink-x4-pro', 'core', ['time.example.invalid'], root / name,
                    mirror_volume_instance=selected)
            receipt = build('good', volume); result = p.decode((root / 'good/native-packages-v1.json').read_bytes())
            assert receipt['mirror_volume_instance'] == volume and receipt['mirror_count'] == 2
            mirrors = result['boot_profiles']['core']['mirrors']
            assert len(mirrors) == 2 and {m['driver'] for m in mirrors} == {True, False}
            for mirror in mirrors:
                asset = (root / 'good/assets' / mirror['url'].split('/')[-1]).read_bytes()
                assert len(asset) == mirror['size'] and p.sha(asset) == mirror['sha256']
            image_name = result['boot_profiles']['core']['profile']['image']['url'].split('/')[-1]
            published = (root / 'good/assets' / image_name).read_bytes()
            assert published == image and helpers.read_image(published, len(image)) == files
            assert p.store_files(store) == files and (files, records) == before
            # No mutation to image_source, any grant, manifests or store bytes.
            (args.output / f'volume-{volume}.json').write_bytes((root / 'good/native-packages-v1.json').read_bytes())
            again = build('repeat', volume); assert again == receipt
            assert (root / 'repeat/native-packages-v1.json').read_bytes() == (root / 'good/native-packages-v1.json').read_bytes()
            for selected in (None, 12 if volume == 9 else 9):
                try: build('rejected', selected)
                except ValueError: pass
                else: raise AssertionError(('selection admitted', selected))
                assert not (root / 'rejected').exists() and p.store_files(store) == files
            runs.append({'volume': volume, 'mirrors': len(mirrors), 'exact_boot_grants': True,
                         'deterministic_feed_sha256': receipt['feed_sha256']})
        bad = [(None, records), (0, records), (-1, records), (True, records), ('9', records),
               (0x80000000, records), (12 if volume == 9 else 9, records)]
        mixed = copy.deepcopy(records); mixed[-1][1]['image_source']['volume_instance'] = 12 if volume == 9 else 9
        bad.append((volume, mixed))
        absent = [r for r in records if r[2]['id'] != 'volume']; bad.append((volume, absent))
        duplicate = copy.deepcopy(records); extra = copy.deepcopy(next(r for r in records if r[2]['id'] == 'volume'))
        extra[2]['id'] = 'other-volume'; duplicate.append(extra)
        bad.append((volume, duplicate))
        mismatch = copy.deepcopy(records); other = copy.deepcopy(extra)
        other[1]['instance_id'] = 12 if volume == 9 else 9; mismatch.append(other)
        bad.append((other[1]['instance_id'], mismatch))
        wrong_api = copy.deepcopy(records)
        next(r for r in wrong_api if r[2]['id'] == 'volume')[2]['provides'][0]['api'] = 2
        bad.append((volume, wrong_api))
        for selected, rows in bad:
            snapshot = copy.deepcopy((rows, files))
            try: feed.mirror_assets(p, rows, files, BASE, selected)
            except ValueError: pass
            else: raise AssertionError(('invalid mirror selection accepted', selected))
            assert (rows, files) == snapshot
        external = copy.deepcopy(records)
        for _, row, _, _ in external:
            if 'image_source' in row: row['image_source']['internal_fallback'] = False
        assert feed.mirror_assets(p, external, files, BASE) == ([], {})
        runs[-1]['rejected_policy_cases'] = len(bad)
    receipt = {'passed': True, 'runs': runs, 'hardware': False, 'product_admission': False,
               'actual_runtime_image_decoder': True, 'actual_runtime_capacity_checker': True,
               'source_sha256': {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in (Path(__file__), Path(feed.__file__))}}
    (args.output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Boot feed: selected volumes 9/12, 24 rejected policies, four no-output build refusals, '
          'exact boot/grant bytes and deterministic mirrors PASS')

if __name__ == '__main__': main()
