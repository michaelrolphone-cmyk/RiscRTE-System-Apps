#!/usr/bin/env python3
"""Build the product-owned infrastructure ELFs using a pinned native toolchain."""
import argparse,hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=pathlib.Path);p.add_argument('--cc',required=True);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
 r=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 cxx=a.cc.removesuffix('gcc')+'g++';readelf=a.cc.removesuffix('gcc')+'readelf'
 common=['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-fno-builtin','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-nostdlib','-nostartfiles','-DWITH_SHA256','-DARDUINOJSON_ENABLE_STD_STRING=0','-DARDUINOJSON_ENABLE_STD_STREAM=0','-Wno-deprecated-declarations']
 includes=[r/'sdk/driver',r/'sdk/app',r/'sdk/hardware',r/'lib/ArduinoJson/src']
 common += ['-I'+str(i) for i in includes]
 sha=out/'sha2.o';subprocess.run([a.cc,*common,'-std=c11','-c',str(ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c'),'-o',str(sha)],check=True)
 targets={'infra-node.elf':(['Services/infra_node/service.cpp','Services/infra_node/Node.cpp','Services/infra_node/cxx.cpp'],'t5_driver_get',[str(sha)]),'infra-station.elf':(['Services/infra_node/station.cpp'],'t5_driver_get',[]),'default.elf':(['Apps/infra_agent/main.cpp'],'app_main',[])}
 records={}
 for name,(sources,entry,extra) in targets.items():
  export=out/(name+'.map');export.write_text('{ global: '+entry+'; local: *; };\n')
  subprocess.run([cxx,*common,'-std=c++17','-fno-exceptions','-fno-rtti','-shared','-Wl,--hash-style=sysv','-Wl,--version-script='+str(export),*[str(ROOT/s) for s in sources],*extra,'-o',str(out/name)],check=True)
  syms=subprocess.check_output([readelf,'--dyn-syms','--wide',str(out/name)],text=True)
  imports=sorted({line.split()[-1] for line in syms.splitlines() if 'GLOBAL' in line and 'UND' in line})
  allowed={'risc_runtime_get_api','malloc','free','realloc','memcpy','memset','memcmp','strlen','strcmp','strncmp','strcpy','snprintf','strtod','strtoul','strtol','strtoll','strtoull','pow'}
  import re
  exported=set(re.findall(r'ESP_ELFSYM_EXPORT\((__\w+)\)',(r/'lib/elf_loader/src/esp_elf_symbol.c').read_text()))
  if not set(imports)<=allowed|exported:raise RuntimeError('Unexpected imports: '+str(set(imports)-allowed-exported))
  if not any('GLOBAL' in line and 'UND' not in line and line.split()[-1]==entry for line in syms.splitlines()):raise RuntimeError('Missing ELF entry')
  payload=(out/name).read_bytes();records[name]={'sha256':hashlib.sha256(payload).hexdigest(),'size_bytes':len(payload),'imports':imports,'entry':entry}
 (out/'elf-build.json').write_text(json.dumps({'compiler':subprocess.check_output([cxx,'--version'],text=True).splitlines()[0],'files':records},indent=2)+'\n')
 print(json.dumps(records,indent=2))
if __name__=='__main__':main()
