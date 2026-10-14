#!/usr/bin/env python3
"""Build the experimental cached installed-app provider; no firmware activation.

The metadata/admission provider is target-link qualified by this command. It requires the
volume asynchronous suffix; SD discovery never calls the synchronous volume on
the owner. Cache replacement requires the copied native bound app-data async suffix.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def time_server(value):
    """One explicit DNS hostname, bounded like the native HTTP-time contract."""
    allowed = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-'
    if (not 1 <= len(value) <= 253 or any(not 1 <= len(label) <= 63 or
            label.startswith('-') or label.endswith('-') or
            any(c not in allowed for c in label) for label in value.split('.'))):
        raise argparse.ArgumentTypeError('Time server must be one bounded DNS hostname')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-repo', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--internal-packages', action='store_true', help='Explicitly require the native scoped package-volume capability')
    parser.add_argument('--boot-profiles', action='store_true', help='Select preserving next-boot profiles and optional immutable SD mirrors')
    parser.add_argument('--native-feed', action='store_true', help='Select the product-owned native catalog, separate from firmware release-index')
    parser.add_argument('--feed-url', help='Explicit product-owned HTTPS native-package catalog')
    parser.add_argument('--time-server', type=time_server, help='Explicit first-fetch SNTP source; requires boot profiles; never changes civil time')
    parser.add_argument('--feed-product', default='xteink-x4-pro', help='Exact feed product identity')
    args = parser.parse_args()
    if args.time_server and not args.boot_profiles:
        parser.error('--time-server requires --boot-profiles')
    if args.boot_profiles and not args.native_feed:
        parser.error('--boot-profiles requires --native-feed')
    if args.feed_url and not args.native_feed:
        parser.error('--feed-url requires --native-feed')
    if not args.feed_product or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-.' for c in args.feed_product):
        parser.error('Invalid feed product identity')
    feed_url = args.feed_url or 'https://raw.githubusercontent.com/michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO/release-index/native-packages-v1.json'
    if not feed_url.startswith('https://') or any(c in feed_url for c in '\"\\\n\r'):
        parser.error('Feed URL must be an HTTPS URL without compiler quoting characters')
    if args.native_feed and args.feed_product != 'xteink-x4-pro' and not args.feed_url:
        parser.error('A different product requires its own explicit feed URL')
    runtime, out = args.runtime_repo.resolve(), args.output_dir.resolve()
    if args.time_server:
        canonical = runtime/'sdk/driver/RiscHttpTimeV1.h'
        if not canonical.is_file() or canonical.read_bytes() != (ROOT/'lib/PortableApps/include/RiscHttpTimeV1.h').read_bytes():
            parser.error('Time suffix must match the selected Runtime SDK exactly')
    out.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    cxx = cc.removesuffix('gcc')+'g++'
    includes = [runtime/'sdk/app', runtime/'sdk/driver', runtime/'sdk/hardware', runtime/'src', runtime/'lib/ArduinoJson/src', ROOT/'lib/InstalledCatalog/include', ROOT/'lib/PortableApps/include']
    flags = ['-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls', '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror'] + ['-I'+str(i) for i in includes]
    if args.native_feed:
        flags += ['-DINSTALLED_APPS_FEED_URL="'+feed_url+'"',
                  '-DINSTALLED_APPS_FEED_PRODUCT="'+args.feed_product+'"']
    if args.boot_profiles: flags += ['-DINSTALLED_APPS_BOOT_PROFILES=1']
    if args.time_server: flags += ['-DINSTALLED_APPS_TIME_SERVER=\"'+args.time_server+'\"']
    sources = [ROOT/'Services/installed_apps/service.cpp', ROOT/'lib/InstalledCatalog/src/InstalledCatalog.c']
    objects = [out/'service.o', out/'catalog.o']
    commands = [[cxx, '-std=c++17', '-fno-exceptions', '-fno-rtti', '-DARDUINOJSON_USE_DOUBLE=0', '-DARDUINOJSON_USE_LONG_LONG=0', *flags, '-c', str(sources[0]), '-o', str(objects[0])], [cc, '-std=c11', *flags, '-c', str(sources[1]), '-o', str(objects[1])]]
    mapping = out/'exports.map'
    mapping.write_text('{ global: t5_driver_get; local: *; };\n')
    elf = out/'driver.elf'
    commands.append([cxx, '-nostdlib', '-nostartfiles', '-shared', '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), *map(str, objects), '-lgcc', '-o', str(elf)])
    for command in commands:
        subprocess.run(command, check=True)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    allowed = {'malloc', 'realloc', 'free', 'memcpy', 'memmove', 'memset', 'memcmp', 'strlen', 'strcmp', 'strncmp', 'strcpy', 'strcat', 'strchr', 'strrchr', 'snprintf', 'strncpy', 'strtoul', 'strtol', 'strtod', 'memchr', 'strstr'}
    if imports - allowed:
        raise RuntimeError('Unexpected imports: '+repr(sorted(imports-allowed)))
    manifest=json.loads((ROOT/'Services/installed_apps/manifest.json').read_text())
    if args.native_feed: manifest['version']='0.12.0'
    if args.boot_profiles:
        manifest['version']='0.13.0'
        manifest['requires'].append({'capability':'platform.provision-request','api':1})
    if args.time_server: manifest['version']='0.14.0'
    if args.internal_packages: manifest['requires'].append({'capability':'platform.package-volume','api':1})
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    headers = [ROOT/'Services/installed_apps/settings.inc', runtime/'src/runtime/packages/InstalledSettingsCheckpoint.h', runtime/'src/runtime/packages/PagedSettingsCheckpoint.h', ROOT/'Services/installed_apps/provision.inc', runtime/'src/runtime/packages/SdPackageDownload.h', ROOT/'Services/installed_apps/providers.inc', ROOT/'Services/installed_apps/install.inc', runtime/'src/runtime/packages/StagedAppInstall.h', runtime/'sdk/driver/RiscStoragePackageAsyncV1.h', runtime/'sdk/driver/RiscBoundAppDataAsyncV1.h', runtime/'sdk/driver/RiscStorageVolumeAsyncV1.h', runtime/'sdk/driver/RiscInstallationV1.h', runtime/'src/runtime/packages/InstalledAppAsyncDiscovery.h', ROOT/'lib/InstalledCatalog/include/InstalledCatalog.h', ROOT/'lib/PortableApps/include/InstalledAppsV1.h']
    receipt = {'status':'experimental-target-link-only', 'limitations':['No target runtime or hardware execution claimed.', 'Requires the scoped async SD provider; cache persistence and UI integration need separate qualification.'], 'commands':commands, 'imports':sorted(imports), 'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+headers}, 'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(), 'repositories':{str(p):{'head':subprocess.check_output(['git','-C',str(p),'rev-parse','HEAD'],text=True).strip(),'dirty':bool(subprocess.check_output(['git','-C',str(p),'status','--porcelain'],text=True))} for p in [ROOT,runtime]}}
    receipt['boot_profiles']=args.boot_profiles
    receipt['time_server']=args.time_server
    receipt['sources'].update({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Services/installed_apps/http_time.inc',ROOT/'lib/PortableApps/include/RiscHttpTimeV1.h',ROOT/'lib/PortableApps/include/InstalledAppsAutomaticTimeV1.h']})
    receipt['sources'].update({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Services/installed_apps/boot_profile.inc',ROOT/'lib/PortableApps/include/InstalledAppsBootProfileV1.h',ROOT/'lib/PortableApps/include/RiscProvisionRequestV1.h']})
    receipt['native_feed']=args.native_feed
    receipt['feed_url']=feed_url if args.native_feed else None
    receipt['feed_product']=args.feed_product if args.native_feed else None
    receipt['sources'][str(ROOT/'Services/installed_apps/feed.inc')]=hashlib.sha256((ROOT/'Services/installed_apps/feed.inc').read_bytes()).hexdigest()
    receipt['internal_packages']=args.internal_packages
    receipt['sources'].update({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Services/installed_apps/media.inc',ROOT/'Services/installed_apps/automatic.inc',ROOT/'lib/PortableApps/include/InstalledAppsAutomaticV1.h',ROOT/'lib/PortableApps/include/InstalledAppsFeedV1.h',ROOT/'lib/PortableApps/include/InstalledAppsProvisionV1.h',runtime/'sdk/driver/RiscInstallationMediaV1.h',runtime/'sdk/driver/RiscInternalPackageVolumeV1.h']})
    receipt.update({'service_version': manifest['version'],
                    'shared_settings_sd': '#define RISC_INSTALLATION_SHARED_SETTINGS_SIZE' in (runtime/'sdk/driver/RiscInstallationV1.h').read_text(),
                    'shared_settings_root': '/RTECFG', 'idle_settings_maintenance': 'settings_pending-v1'})
    (out/'build.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(json.dumps({'elf':str(elf),'bytes':elf.stat().st_size,'imports':sorted(imports),'status':receipt['status']}))

if __name__ == '__main__':
    main()
