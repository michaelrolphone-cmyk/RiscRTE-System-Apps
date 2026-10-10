"""Small deterministic target-ELF builder for optional scene packages."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'memchr', 'strcmp',
           'strncmp', 'strlen', 'strnlen', 'strchr', 'strcpy', 'snprintf'}


def compiler_path() -> str:
    compiler = (os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
                or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc'))
    if not Path(compiler).is_file():
        raise FileNotFoundError(f'Xtensa compiler not installed: {compiler}')
    return compiler


def json_write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True)+'\n')


def build(compiler: str, include: Path, output: Path, manifest: dict,
          sources: list[Path], defines: list[str] | None = None,
          exports: set[str] | None = None) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    application = manifest['type'] == 'application'
    exports = ({'app_main'} if application else {'t5_driver_get'}) if exports is None else set(exports)
    if not exports or any(not name.isidentifier() or not name.isascii() for name in exports):
        raise ValueError('Exports must be a nonempty set of C symbol names')
    mapping = output/'exports.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    flags = ['-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib',
             '-nostartfiles', '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv',
             '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror']
    elf = output/manifest['file_name']
    subprocess.run([compiler,*flags,'-I'+str(include),*(defines or []),
                    *map(str,sources),'-o',str(elf)],check=True)
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError(f'Not an Xtensa ELF32 ET_DYN module: {elf}')
    symbols = subprocess.check_output([compiler.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
    imports={line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    found={line.split()[-1] for line in symbols.splitlines()
           if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')}
    if imports-IMPORTS or found != exports:
        raise ValueError(f'{elf.name}: invalid imports {imports-IMPORTS} or exports {found}')
    json_write(output/(Path(manifest['file_name']).with_suffix('.json').name if application else 'manifest.json'),manifest)
    receipt={'id':manifest['id'],'version':manifest['version'],
             'file':elf.name,'size_bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),
             'imports':sorted(imports),'exports':sorted(found),
             'compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],
             'defines':defines or [],'physical_testing':'not performed',
             'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(sources)|{q for source in sources for q in source.parent.rglob('*.inc')})},
             'sdk_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(include.glob('*.h'))}}
    json_write(output/'build.json',receipt)
    mapping.unlink()
    return receipt
