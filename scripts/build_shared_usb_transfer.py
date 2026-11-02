#!/usr/bin/env python3
"""Build exactly one USB SD Transfer ELF; no device/profile selectors."""
import argparse,hashlib,json,os,shutil,subprocess,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--msc-sdk',type=Path,required=True)
p.add_argument('--compiler',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);cc=str(a.compiler.resolve());out=a.output
header=a.msc_sdk/'RiscUsbDeviceMscV1.h'
assert digest(header)=='d55a9340c3ff64e8679c4d9332812f073c30b8aa08ae3e3fb04f9010cf517ed6'
mapping=out/'exports.map';mapping.write_text('{global:app_main;app_module_init;app_module_fini;risc_resident_app_descriptor_v1;local:*;};\n')
elf=out/'usb_sd_transfer.elf';dep=out/'dependencies.d'
cmd=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls',
 '-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared',
 '-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),
 '-Wall','-Wextra','-Werror','-DPORTABLE_USB_TRANSFER_SHARED',
 '-I'+str(a.msc_sdk),'-I'+str(ROOT/'sdk/usb'),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'lib/PortableApps/include'),
 '-I'+str(ROOT/'lib/NativeApps/include'),'-MMD','-MF',str(dep),
 str(ROOT/'Apps/usb_sd_transfer.c'),'-lgcc','-o',str(elf)]
subprocess.run(cmd,check=True)
nm=cc.removesuffix('gcc')+'nm'
symbols=subprocess.check_output([nm,'-D',str(elf)],text=True)
imports={r.split()[-1] for r in symbols.splitlines() if ' U ' in ' '+r}
exports={r.split()[-1] for r in symbols.splitlines() if len(r.split())>=3 and r.split()[-2] in ('T','D','B','R')}
assert exports=={'app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'}
assert imports<={'risc_runtime_get_api','memcpy','memset','strlen','snprintf','strcmp'}
validator=out/'validate-elf'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),
 '-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),
 str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(elf)],check=True)
manifest=ROOT/'Apps/shared_usb_sd_transfer.json';shutil.copyfile(manifest,out/'usb_sd_transfer.json')
record={'schema':1,'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],text=True,cwd=ROOT).strip(),
 'sha256':digest(elf),'bytes':elf.stat().st_size,'manifest_sha256':digest(manifest),
 'compiler':subprocess.check_output([cc,'--version'],text=True),'compiler_sha256':digest(Path(cc)),
 'command':cmd,'imports':sorted(imports),'exports':sorted(exports),'physical_testing':False,
 'source_sha256':{str(f.relative_to(ROOT)):digest(f) for f in
 [ROOT/'Apps/usb_sd_transfer.c',ROOT/'Apps/usb_sd_transfer_shared.inc',manifest,Path(__file__)]},
 'msc_header_sha256':digest(header),
 'dependencies':{str(Path(x).resolve()):digest(Path(x)) for x in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1])}}
(out/'build.json').write_text(json.dumps(record,indent=2)+'\n')
shutil.copyfile(ROOT/'LICENSE',out/'LICENSE')
print(json.dumps(record,indent=2))
