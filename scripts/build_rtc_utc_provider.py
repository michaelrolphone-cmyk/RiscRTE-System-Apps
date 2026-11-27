#!/usr/bin/env python3
"""Build an explicitly based rtc.clock@2 UTC-suffix package without changing
the original product recipe or adding a driver/capability to its graph."""
import argparse, hashlib, json, os, re, shlex, subprocess
from pathlib import Path
import scene_build

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'manifest', 'runtime', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--include', type=Path, action='append', default=[])
    p.add_argument('--offset-seconds', type=int, required=True)
    p.add_argument('--version', required=True)
    a = p.parse_args()
    if not -86400 <= a.offset_seconds <= 86400:
        raise ValueError('Explicit RTC basis is outside one day')
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', a.version):
        raise ValueError('Expected explicit package semantic version')
    manifest = json.loads(a.manifest.read_text())
    if (manifest.get('type') != 'driver' or manifest.get('driver_abi') != 2 or
        manifest.get('architecture') != 'xtensa-esp32s3' or
        manifest.get('provides') != [{'capability':'rtc.clock', 'api':2}] or
        manifest.get('file_name') != 'driver.elf'):
        raise ValueError('Expected the original single rtc.clock@2 driver manifest')
    manifest['version'] = a.version
    system = Path(__file__).resolve().parents[1]
    source = a.source.resolve(strict=True)
    includes = [system/'sdk/driver', *[x.resolve(strict=True) for x in a.include]]
    flags = ['-DRTC_UTC_OFFSET_SECONDS='+str(a.offset_seconds),
             *['-I'+str(x) for x in includes]]
    cc = scene_build.compiler_path()
    a.output.mkdir(parents=True, exist_ok=False)
    preprocessing = ['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls',
                     '-fvisibility=hidden','-ffreestanding','-fno-builtin',
                     '-nostdlib','-nostartfiles','-shared','-Wall','-Wextra','-Werror']
    dep = subprocess.check_output([cc, *preprocessing, '-I'+str(system/'sdk/app'), *flags,
                                   str(source), '-MM'], text=True)
    paths = [Path(x).resolve() for x in shlex.split(dep.replace('\\\n',' ').split(':',1)[1])]
    paths += [Path(__file__).resolve(), a.manifest.resolve(),
              system/'scripts/scene_build.py', system/'scripts/native_toolchain.py']
    inputs = {str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}
    builds = [scene_build.build(cc, system/'sdk/app', a.output/name, manifest,
                               [source], flags) for name in ('package','repeat')]
    if builds[0]['sha256'] != builds[1]['sha256']:
        raise ValueError('Provider repeat differs')
    validator = a.output/'validate'
    fixture = a.output/'validate.c'
    fixture.write_text('#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n'
        'extern bool esp_elf_validate_file(const uint8_t*,size_t);\n'
        'int main(int argc,char**argv){if(argc!=2)return 2;FILE*f=fopen(argv[1],"rb");'
        'if(!f)return 2;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n<=0)return 2;'
        'uint8_t*p=malloc((size_t)n);if(!p)return 2;if(fread(p,1,(size_t)n,f)!=(size_t)n)return 2;'
        'fclose(f);bool ok=esp_elf_validate_file(p,(size_t)n);free(p);return ok?0:1;}\n')
    (a.output/'sdkconfig.h').write_text('#pragma once\n#define CONFIG_ELF_LOADER 1\n'
        '#define CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT 1\n#define CONFIG_SPIRAM 1\n'
        '#define CONFIG_ELF_LOADER_LOAD_PSRAM 1\n#define CONFIG_IDF_TARGET_ESP32S3 1\n')
    host = os.environ.get('CC','cc')
    flags_host = ['-std=c11','-Wall','-Wextra','-Werror','-I'+str(a.output),
                  '-I'+str(a.runtime/'lib/elf_loader/include')]
    validator_source = a.runtime/'lib/elf_loader/src/esp_elf_validate.c'
    for unit in (fixture, validator_source):
        raw = subprocess.check_output([host,*flags_host,'-MM',str(unit)],text=True)
        for name in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]):
            path = Path(name).resolve()
            if path not in paths:
                paths.append(path);inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    subprocess.run([host,*flags_host,str(fixture),str(validator_source),'-o',str(validator)],check=True)
    subprocess.run([str(validator),str(a.output/'package/driver.elf')],check=True)
    if inputs != {str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}:
        raise ValueError('Provider inputs changed during build')
    (a.output/'receipt.json').write_text(json.dumps({'builds':builds,
        'offset_seconds':a.offset_seconds, 'inputs':inputs,
        'target_preprocessing_flags':preprocessing,
        'compiler_executable':str(Path(cc).resolve()),
        'compiler_sha256':hashlib.sha256(Path(cc).read_bytes()).hexdigest(),
        'original_manifest_sha256':hashlib.sha256(a.manifest.read_bytes()).hexdigest(),
        'scope':'Explicit-basis repeat target and selected Runtime structural validator; no physical proof'}, indent=2))

if __name__ == '__main__':
    main()
