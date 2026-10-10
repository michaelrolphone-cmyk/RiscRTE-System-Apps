#!/usr/bin/env python3
"""Compile actual freestanding Xtensa objects; this is not a deployable app."""
import argparse,hashlib,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--toolchain',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);commands=[]
modules=['WebDavCore','WebDavProperties','WebDavSession','WebDavDigest','WebDavSharing']
sha=out/'sha2.o'
cmd=[str(a.toolchain/'xtensa-esp32s3-elf-gcc'),'-std=c11','-Os','-mlongcalls','-fPIC','-Wall','-Wextra','-Werror','-c','lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c','-o',str(sha)]
subprocess.run(cmd,cwd=root,check=True);commands.append(cmd)
for name in modules:
 cmd=[str(a.toolchain/'xtensa-esp32s3-elf-g++'),'-std=c++17','-Os','-mlongcalls','-fPIC','-fno-exceptions','-fno-rtti','-fno-common','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror','-Ilib/PortableApps/include','-c',f'lib/RemoteFiles/{name}.cpp','-o',str(out/(name+'.o'))]
 subprocess.run(cmd,cwd=root,check=True);commands.append(cmd)
cmd=[str(a.toolchain/'xtensa-esp32s3-elf-ld'),'-r',*[str(out/(n+'.o')) for n in modules],str(sha),'-o',str(out/'protocol.o')];subprocess.run(cmd,check=True);commands.append(cmd)
imports=subprocess.check_output([str(a.toolchain/'xtensa-esp32s3-elf-nm'),'-u',str(out/'protocol.o')],text=True)
names={line.split()[-1] for line in imports.splitlines()};allowed={'memcpy','memset','memcmp','memchr','strlen','strcmp','strncmp','strcpy','strchr','strrchr','strstr','snprintf'}
assert names<=allowed, names-allowed
receipt={'schema':1,'kind':'Xtensa production protocol object compile, not installed app ELF','source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'commands':commands,'imports':sorted(names),'objects':{v.name:{'bytes':v.stat().st_size,'sha256':hashlib.sha256(v.read_bytes()).hexdigest()} for v in out.glob('*.o')}}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Xtensa protocol objects and bounded libc imports PASS: '+', '.join(sorted(names)))
