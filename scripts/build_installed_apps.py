#!/usr/bin/env python3
"""Build the experimental cached installed-app provider; no firmware activation.

The metadata/admission provider is target-link qualified by this command. It requires the
volume asynchronous suffix; SD discovery never calls the synchronous volume on
the owner. Internal cache persistence still uses the existing app-data backend.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-repo', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    runtime, out = args.runtime_repo.resolve(), args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    cxx = cc.removesuffix('gcc')+'g++'
    includes = [runtime/'sdk/app', runtime/'sdk/driver', runtime/'sdk/hardware', runtime/'src', runtime/'lib/ArduinoJson/src', ROOT/'lib/InstalledCatalog/include', ROOT/'lib/PortableApps/include']
    flags = ['-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls', '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror'] + ['-I'+str(i) for i in includes]
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
    allowed = {'malloc', 'realloc', 'free', 'memcpy', 'memmove', 'memset', 'memcmp', 'strlen', 'strcmp', 'strncmp', 'strcpy', 'strchr', 'strrchr', 'snprintf', 'strncpy', 'strtoul', 'strtol', 'strtod', 'memchr'}
    if imports - allowed:
        raise RuntimeError('Unexpected imports: '+repr(sorted(imports-allowed)))
    shutil.copyfile(ROOT/'Services/installed_apps/manifest.json', out/'manifest.json')
    headers = [runtime/'sdk/driver/RiscInstallationV1.h', runtime/'src/runtime/packages/InstalledAppAsyncDiscovery.h', ROOT/'lib/InstalledCatalog/include/InstalledCatalog.h', ROOT/'lib/PortableApps/include/InstalledAppsV1.h']
    receipt = {'status':'experimental-target-link-only', 'limitations':['No target runtime or hardware execution claimed.', 'Requires the scoped async SD provider; cache persistence and UI integration need separate qualification.'], 'commands':commands, 'imports':sorted(imports), 'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+headers}, 'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(), 'repositories':{str(p):{'head':subprocess.check_output(['git','-C',str(p),'rev-parse','HEAD'],text=True).strip(),'dirty':bool(subprocess.check_output(['git','-C',str(p),'status','--porcelain'],text=True))} for p in [ROOT,runtime]}}
    (out/'build.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(json.dumps({'elf':str(elf),'bytes':elf.stat().st_size,'imports':sorted(imports),'status':receipt['status']}))

if __name__ == '__main__':
    main()
