#!/usr/bin/env python3
"""Freeze a credential-free core boot feed from an already admitted product store.

Offline byte custody only: this neither admits a native graph/ELF nor builds,
changes, publishes or installs firmware. Use the exact qualified store, bootfs,
and native firmware together. The caller owns immutable hosting at --base-url.
"""
import argparse
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
from types import SimpleNamespace

MAX_FILES = 160
MAX_PROFILE = 16384
# Match the service's public-profile bound. This covers JSON escaping when it
# injects the owner's maximum 32-byte SSID and 63-byte printable password.
WIFI_HEADROOM = 512
MAX_FEED = 65536
MAX_MIRRORS = 64
MAX_MIRROR_BYTES = 2097152
NATIVE_KEYS = {'layout', 'runtime_version', 'store_abi', 'firmware_size', 'firmware_sha256'}
COHORT_KEYS = NATIVE_KEYS | {'schema', 'schema_version', 'product', 'version',
                             'source_repo', 'source_revision'}
LAYOUT_ABI = {'riscrte-paired-16m-v1': 1, 'riscrte-paired-appdata-v2': 2,
              'riscrte-paired-appdata-v3': 3}
TARGETS = {'riscrte-paired-16m-v1': 'esp32s3-16mb-paired',
           'riscrte-paired-appdata-v2': 'esp32s3-16mb-appdata',
           'riscrte-paired-appdata-v3': 'esp32s3-16mb-appdata-expanded'}
CREDENTIAL_KEYS = {'wifi', 'ssid', 'password', 'passphrase', 'psk', 'credentials',
                   'private_key', 'api_key', 'access_token', 'refresh_token'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def matches(pattern, value):
    return isinstance(value, str) and re.fullmatch(pattern, value) is not None


def load_runtime(runtime):
    """Load the selected Runtime's helpers, without reusing another checkout."""
    scripts = Path(runtime).absolute() / 'scripts'
    require(not any(path.is_symlink() for path in (scripts, *scripts.parents)),
            'Runtime helper path must not contain symlinks')
    names = ('check_versions', 'release_assets', 'provision_profile',
             'spiffs_image', 'store_image_capacity')
    previous = {name: sys.modules.get(name) for name in names}
    loaded = {}
    try:
        for name in names:
            path = scripts / (name + '.py')
            require(path.is_file() and not path.is_symlink(), 'Runtime helper missing or unsafe')
            spec = importlib.util.spec_from_file_location(name, path)
            module = importlib.util.module_from_spec(spec)
            sys.modules[name] = module
            spec.loader.exec_module(module)
            loaded[name] = module
    finally:
        for name, module in previous.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module
    p = loaded['provision_profile']
    require(p.MAX_FILES == MAX_FILES and p.MAX_PROFILE == MAX_PROFILE,
            'Runtime must support compact schema 4 and 160 files')
    return SimpleNamespace(profile=p, read_image=loaded['spiffs_image'].read_image,
                           inspect_image=loaded['store_image_capacity'].inspect_image)


def no_credentials(value):
    """Do not accept owner Wi-Fi or credential fields in public JSON inputs."""
    if isinstance(value, dict):
        require(not any(key.lower().replace('-', '_') in CREDENTIAL_KEYS for key in value),
                'credentials forbidden in public input')
        for child in value.values():
            no_credentials(child)
    elif isinstance(value, list):
        for child in value:
            no_credentials(child)


def source(p, url, directory=False):
    p.source(url, directory)
    host = url[8:].split('/', 1)[0]
    require(len(host) <= 253 and all(matches(r'[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?', label)
                                   for label in host.split('.')), 'invalid HTTPS host')
    return url


def time_input(p, servers):
    require(isinstance(servers, (list, tuple)) and 1 <= len(servers) <= 3,
            'one to three explicit SNTP servers required')
    require(all(matches(r'[A-Za-z0-9-](?:[A-Za-z0-9.-]{0,61}[A-Za-z0-9-])?', server)
                for server in servers), 'invalid SNTP server')
    require(len(set(servers)) == len(servers), 'duplicate SNTP server')
    value = {'schema': 'riscrte.sntp', 'schema_version': 1, 'servers': list(servers)}
    require(len(p.encode(value)) <= 384, 'SNTP input exceeds native bound')
    return value


def native_identity(p, cohort, firmware, product):
    require(isinstance(cohort, dict) and set(cohort) == COHORT_KEYS, 'exact cohort fields required')
    require(cohort['schema'] == 'riscrte.cohort' and
            type(cohort['schema_version']) is int and cohort['schema_version'] == 1,
            'unsupported cohort schema')
    require(cohort['product'] == product, 'cohort product mismatch')
    require(matches(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', cohort['source_repo']) and
            len(cohort['source_repo']) < 128 and '..' not in cohort['source_repo'] and
            matches(r'[0-9a-f]{40}', cohort['source_revision']), 'cohort source identity')
    require(all(matches(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', cohort[key]) and
                len(cohort[key]) < 32 and all(int(part) <= 0xffffffff for part in cohort[key].split('.'))
                for key in ('version', 'runtime_version')), 'cohort version identity')
    layout = cohort['layout']
    require(isinstance(layout, str) and layout in LAYOUT_ABI and
            type(cohort['store_abi']) is int and cohort['store_abi'] == LAYOUT_ABI[layout],
            'cohort layout or ABI mismatch')
    maximum = 0x300000 if cohort['store_abi'] == 1 else 0x260000
    require(type(cohort['firmware_size']) is int and 32 <= len(firmware) <= maximum and
            cohort['firmware_size'] == len(firmware) and
            matches(r'[0-9a-f]{64}', cohort['firmware_sha256']) and
            cohort['firmware_sha256'] == p.sha(firmware), 'cohort firmware binding mismatch')
    for prefix, expected in ((b'RISC_RUNTIME_VERSION:', cohort['runtime_version'].encode()),
                             (b'RISC_PAIRED_STORE_ABI:', str(cohort['store_abi']).encode())):
        require(set(re.findall(re.escape(prefix) + rb'([^\0]*)\0', firmware)) == {expected},
                'native compiled version or ABI mismatch')
    return {key: cohort[key] for key in sorted(NATIVE_KEYS)}


def store_closure(p, files):
    """Resolve only boot-admitted manifests and their internal image bytes.

    Preserve additional admitted assets (icons, fonts, data) byte-for-byte.
    This is a publication reference check, not a replacement for product/native
    capability, grant, hardware, manifest, or ELF admission.
    """
    documents = {name: p.decode(data) for name, data in files.items() if name.endswith('.json')}
    for value in documents.values():
        no_credentials(value)
    boot = documents.get('boot.json')
    required = {'board', 'default_app', 'drivers', 'app_capabilities'}
    require(isinstance(boot, dict) and required <= set(boot) and
            set(boot) <= required | {'port', 'cohort_migration', 'provider_activation',
                                     'resident_shell', 'installation'}, 'unsupported boot fields')
    require(boot['board'] == 'board.json' and boot['default_app'] == 'default.elf',
            'complete default boot identity required')
    expected = {'boot.json', 'board.json', 'cohort.json'}
    records = []
    default = False
    identities = {False: set(), True: {}}
    for driver, group in ((False, 'app_capabilities'), (True, 'drivers')):
        rows = boot[group]
        require(isinstance(rows, list) and len(rows) <= MAX_FILES, 'invalid boot records')
        for row in rows:
            require(isinstance(row, dict) and 'manifest' in row, 'invalid admitted record')
            manifest_name = p.relative(row['manifest'])
            manifest = documents.get(manifest_name)
            require(isinstance(manifest, dict), 'admitted manifest missing')
            require(manifest.get('type') == ('driver' if driver else 'application') and
                    matches(r'[A-Za-z0-9_-][A-Za-z0-9_.-]{0,62}', manifest.get('id')) and
                    '..' not in manifest['id'], 'admitted manifest identity')
            # A legacy external-only app may not have this file in SPIFFS at
            # all. Apply the native filename bound here; actual store entries
            # have already passed the stricter physical SPIFFS name bound.
            filename = p.relative(manifest.get('file_name'), 127)
            require('/' not in filename and filename.endswith('.elf'), 'internal image filename')
            internal = p.relative(str(PurePosixPath(manifest_name).parent / filename), 191)
            removable = image_source(p, row)
            fallback = removable is None or removable.get('internal_fallback') is True
            require(internal in files or not fallback, 'internal fallback image missing')
            if driver:
                instance = row.get('instance_id', 0)
                require(type(instance) is int and 0 <= instance <= 0xffffffff, 'driver instance')
                identity = (manifest['id'], instance)
                require(identity not in identities[True], 'duplicate admitted driver')
                identities[True][identity] = manifest_name
            else:
                require(manifest['id'] not in identities[False], 'duplicate admitted app')
                identities[False].add(manifest['id'])
                default |= internal == 'default.elf' and internal in files
            if internal in files:
                for key, actual in (('size_bytes', len(files[internal])), ('sha256', p.sha(files[internal]))):
                    if key in manifest:
                        require(type(manifest[key]) is type(actual) and manifest[key] == actual,
                                'manifest internal image binding mismatch')
                expected.add(internal)
            expected.add(manifest_name)
            records.append((driver, row, manifest, internal))
    require(default, 'default app must be boot-admitted')
    require(expected <= set(files), 'missing store files from admitted closure')
    return documents['cohort.json'], records


def image_source(p, row):
    if 'image_source' not in row:
        return None
    item = row['image_source']
    require(isinstance(item, dict) and {'volume_instance', 'path'} <= set(item) and
            set(item) <= {'volume_instance', 'path', 'internal_fallback'} and
            type(item['volume_instance']) is int and 0 <= item['volume_instance'] <= 0x7fffffff and
            ('internal_fallback' not in item or type(item['internal_fallback']) is bool),
            'invalid mirror/source policy')
    p.relative(item['path'], 191)
    return item


def mirror_assets(p, records, files, base_url):
    mirrors = {}
    assets = {}
    for driver, row, manifest, internal in records:
        item = image_source(p, row)
        # Existing external-only policies remain untouched in the exact image.
        # They do not authorize publication of a new internal-fallback mirror.
        if item is None or item.get('internal_fallback') is not True:
            continue
        require(item['volume_instance'] == 12, 'mirror requires SD12 and explicit internal fallback')
        data = files[internal]
        require(0 < len(data) <= MAX_MIRROR_BYTES, 'mirror image exceeds consumer bound')
        require(matches(r'[a-z0-9_-]+', manifest['id']), 'unsupported mirror package ID')
        digest = p.sha(data)
        name = 'image-' + digest + '.elf'
        relative = ('Drivers/' if driver else 'Apps/') + manifest['id'] + '/' + name
        # Boot image_source paths are relative. Feed mirror paths are absolute.
        require(item['path'] == relative, 'mirror path must match admitted content-addressed identity')
        p.relative(relative, 191)
        path = '/' + relative
        descriptor = {'driver': driver, 'path': path, 'url': source(p, base_url + name),
                      'size': len(data), 'sha256': digest}
        require(path not in mirrors or mirrors[path] == descriptor, 'conflicting mirror identity')
        mirrors[path] = descriptor
        require(name not in assets or assets[name] == data, 'conflicting mirror asset')
        assets[name] = data
    require(len(mirrors) <= MAX_MIRRORS, 'too many boot profile mirrors')
    return [mirrors[path] for path in sorted(mirrors)], assets


def build(runtime, store, image, firmware, base_url, product, profile, servers, output):
    require(matches(r'[a-z0-9][a-z0-9.-]{0,62}', product), 'explicit product identity required')
    require(profile == 'core', 'only the explicit core profile is supported')
    helpers = load_runtime(runtime)
    p = helpers.profile
    # Reject symlinks before canonicalizing '..' aliases for the containment
    # check; aliases must not turn an output into part of the input snapshot.
    output = p.destination(output).resolve()
    store = p.safe_path(store).resolve()
    require(not (output == store or store in output.parents or output in store.parents),
            'output must be separate from input store')
    source(p, base_url, True)
    timing = time_input(p, servers)
    files = p.store_files(store)
    cohort, records = store_closure(p, files)
    native = p.read(firmware, 0x300000)
    identity = native_identity(p, cohort, native, product)
    raw = p.read(image, 8 * 1024 * 1024)
    partition = p.LAYOUTS[identity['layout']]
    helpers.inspect_image(raw, partition)
    require(helpers.read_image(raw, partition) == files, 'image and complete product store differ')
    image_name = 'bootfs-' + p.sha(raw) + '.bin'
    inventory = {'schema': 'riscrte.provisioning-inventory', 'schema_version': 2,
                 'layout': identity['layout'], 'runtime_target': TARGETS[identity['layout']],
                 'image': {'url': source(p, base_url + image_name), 'bytes': len(raw), 'sha256': p.sha(raw)},
                 'files': [{'path': name, 'bytes': len(data), 'sha256': p.sha(data)}
                           for name, data in sorted(files.items())]}
    p.verify_inventory(inventory, files)
    public_profile = {'schema': 'riscrte.provisioning', 'schema_version': 4,
                      'image': inventory['image'],
                      'files': [[entry[key] for key in ('path', 'bytes', 'sha256')]
                                for entry in inventory['files']]}
    profile_size = len(p.encode(public_profile))
    require(profile_size <= MAX_PROFILE - WIFI_HEADROOM,
            'public profile leaves insufficient owner Wi-Fi headroom')
    mirrors, assets = mirror_assets(p, records, files, base_url)
    assets[image_name] = raw
    feed = {'schema': 'riscrte.native-packages', 'version': 1, 'product': product,
            'apps': [], 'profiles': {profile: []},
            'boot_profiles': {profile: {'native': identity, 'profile': public_profile,
                                        'time': timing, 'mirrors': mirrors}}}
    no_credentials(feed)
    encoded = p.encode(feed)
    require(len(encoded) <= MAX_FEED, 'feed exceeds 64 KiB')
    receipt = {'schema': 'riscrte.boot-profile-feed-receipt', 'schema_version': 1,
               'product': product, 'profile': profile, 'file_count': len(files),
               'profile_bytes': profile_size, 'wifi_headroom': MAX_PROFILE - profile_size,
               'feed_sha256': p.sha(encoded), 'feed_bytes': len(encoded),
               'assets': {name: {'size': len(data), 'sha256': p.sha(data)}
                          for name, data in sorted(assets.items())},
               'credential_free': True, 'published': False}
    # Refuse a changed input snapshot before creating any output.
    require(p.store_files(store) == files and p.read(image, 8 * 1024 * 1024) == raw and
            p.read(firmware, 0x300000) == native, 'inputs changed during preparation')
    output.mkdir()
    try:
        (output / 'assets').mkdir()
        for name, data in sorted(assets.items()):
            p.write(output / 'assets' / name, data)
            require(p.read(output / 'assets' / name) == data, 'asset output readback mismatch')
        p.write(output / 'native-packages-v1.json', encoded)
        require(p.read(output / 'native-packages-v1.json', MAX_FEED) == encoded, 'feed output readback mismatch')
        p.write(output / 'receipt.json', p.encode(receipt))
        p.write(output / 'SHA256SUMS', ''.join(
            f'{p.sha(p.read(path))}  {path.relative_to(output).as_posix()}\n'
            for path in sorted(output.rglob('*')) if path.is_file()).encode())
        p.write(output / 'COMPLETE', b'riscrte.boot-profile-feed.v1\n')
    except BaseException:
        shutil.rmtree(output)
        raise
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('runtime', 'store', 'image', 'firmware', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--base-url', required=True, help='Immutable HTTPS directory ending in / for assets/')
    parser.add_argument('--product', required=True)
    parser.add_argument('--profile', required=True, choices=['core'])
    parser.add_argument('--time-server', action='append', required=True, dest='servers')
    args = parser.parse_args(argv)
    try:
        receipt = build(**vars(args))
    except (Exception, KeyboardInterrupt):
        # Inputs may accidentally contain secrets; never echo raw input/errors.
        print('Boot feed preparation failed; verify admitted inputs and Runtime helpers.', file=sys.stderr)
        return 1
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
