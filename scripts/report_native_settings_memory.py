#!/usr/bin/env python3
"""Report exact ELF sections, pinned loader allocation requests and bounded admission.

This never loads Xtensa instructions or qualifies linked-device memory fit.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

RUNTIME_PIN = '48229bd1593a2e24e95bf4537b495ee615990fd3'
RETAINED_SIZE = 157124
RETAINED_SHA256 = 'a7558b3bd2cd0c2a0edee95e4a8a69477a63772e8300e33e8e7332a2558f8d41'
RUNTIME_INPUTS = (
    'lib/elf_loader/src/esp_elf_validate.c', 'lib/elf_loader/src/esp_elf.c',
    'lib/elf_loader/src/esp_elf_adapter.c', 'lib/elf_loader/src/dlso/dlmod.c',
    'lib/elf_loader/include/private/elf_types.h',
    'lib/elf_loader/include/private/esp_elf_data_layout.h',
    'src/runtime/packages/ElfAdmission.h', 'test/native_apps/stubs/sdkconfig.h',
)
DATA_ORDER = ('.data', '.rodata', '.data.rel.ro', '.bss')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def inspect(path):
    require(52 <= path.stat().st_size <= 8*1024*1024, 'ELF exceeds bounded input size')
    data = path.read_bytes()
    h = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    require(h[0][:7] == b'\x7fELF\x01\x01\x01' and h[1:3] == (3, 94),
            'Expected ELF32 little-endian Xtensa shared object')
    require(h[11] == 40 and 0 < h[12] <= 256 and h[13] < h[12], 'Invalid section table')
    require(h[6]+h[11]*h[12] <= len(data), 'Truncated section table')
    sections = [dict(zip(('name_offset','type','flags','address','offset','size_bytes',
                         'link','info','alignment','entry_size'),
                        struct.unpack_from('<10I',data,h[6]+40*i))) for i in range(h[12])]
    names = sections[h[13]]
    require(names['offset']+names['size_bytes'] <= len(data), 'Truncated section names')
    strings = data[names['offset']:names['offset']+names['size_bytes']]
    for i, s in enumerate(sections):
        require(s['name_offset'] < len(strings), 'Invalid section name')
        s['name'] = strings[s['name_offset']:].split(b'\0',1)[0].decode('ascii')
        s['index'] = i
    require(h[10] <= 128 and h[9] == 32 and h[5]+32*h[10] <= len(data), 'Invalid program headers')
    segments = [dict(zip(('type','offset','virtual_address','physical_address','file_bytes',
                         'memory_bytes','flags','alignment'),
                        struct.unpack_from('<8I',data,h[5]+32*i))) for i in range(h[10])]
    load = [s for s in segments if s['type'] == 1]
    named = {s['name']:s for s in sections}
    require('.text' in named and named['.text']['size_bytes'], 'Missing text')
    data_rows = []
    for name in DATA_ORDER:
        if name not in named or not named[name]['size_bytes']:
            continue
        s = named[name]; alignment = max(4,s['alignment'])
        require(not (alignment & (alignment-1)) and not s['address'] % max(1,s['alignment']),
                'Invalid loader data alignment')
        data_rows.append({'section':name,'payload_bytes':s['size_bytes'],
                          'alignment':alignment,'reserved_padding_bytes':alignment-1})
    text_bytes = named['.text']['size_bytes']
    data_bytes = sum(s['payload_bytes']+s['reserved_padding_bytes'] for s in data_rows)
    return {'sha256':sha(data),'file_size_bytes':len(data),'sections':sections,
        'program_headers':segments,'shf_alloc_size_sum_bytes':sum(s['size_bytes'] for s in sections if s['flags']&2),
        'pt_load':{'file_size_sum_bytes':sum(s['file_bytes'] for s in load),
                   'memory_size_sum_bytes':sum(s['memory_bytes'] for s in load),
                   'virtual_span_bytes':max(s['virtual_address']+s['memory_bytes'] for s in load)-min(s['virtual_address'] for s in load)},
        'section_loader':{'selection':'CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1',
            'executable_allocation_request_bytes':text_bytes,'data_allocation_request_bytes':data_bytes,
            'data_allocation_components':data_rows,'resident_section_allocation_request_bytes':text_bytes+data_bytes,
            'uncached_input_plus_section_allocations_bytes':len(data)+text_bytes+data_bytes,
            'structural_limit_bytes':8*1024*1024,
            'within_structural_limit':text_bytes+data_bytes<=8*1024*1024,
            'scope':'Allocation payload requests, excluding exported-symbol tables/names, module bookkeeping, allocator overhead, caches, stacks, runtime/provider memory and app dynamic allocations'},
        'segment_loader':{'selection':'CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=0',
            'executable_allocation_request_bytes':max(s['virtual_address']+s['memory_bytes'] for s in load)-min(s['virtual_address'] for s in load)},
        'ordinary_application_file_limit_bytes':2097152,
        'linked_device_memory_fit_qualified':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf',type=Path,required=True)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True,help='New or empty evidence directory')
    parser.add_argument('--retained-elf',type=Path,help='Optional exact retained Wave.59 Settings ELF')
    args=parser.parse_args();out=args.output.resolve()
    require(not out.exists() or not any(out.iterdir()),'Refuse stale evidence: output directory must be empty')
    out.mkdir(parents=True,exist_ok=True)
    runtime=out/'runtime';hashes={}
    for path in RUNTIME_INPUTS:
        content=subprocess.check_output(['git','-C',str(args.runtime),'show',RUNTIME_PIN+':'+path])
        dest=runtime/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(content);hashes[path]=sha(content)
    (out/'admit.cpp').write_text(ADMISSION_SOURCE)
    images={'native':args.elf.resolve()};records={'native':inspect(images['native'])}
    if args.retained_elf:
        images['retained059']=args.retained_elf.resolve();records['retained059']=inspect(images['retained059'])
        require(records['retained059']['sha256']==RETAINED_SHA256 and records['retained059']['file_size_bytes']==RETAINED_SIZE,
                'Retained ELF does not match the delivered.59 identity')
    include=['-I'+str(runtime/'test/native_apps/stubs'),'-I'+str(runtime/'lib/elf_loader/include'),'-I'+str(runtime/'src')]
    runs=[]
    for sanitized in (False,True):
        flags=['-O1','-g','-Wall','-Wextra','-Werror']
        if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
        obj=out/f'validate-{int(sanitized)}.o';binary=out/f'admit-{int(sanitized)}'
        commands=[[os.environ.get('CC','cc'),'-std=c11',*flags,*include,'-c',str(runtime/RUNTIME_INPUTS[0]),'-o',str(obj)],
                  [os.environ.get('CXX','c++'),'-std=c++17',*flags,*include,str(out/'admit.cpp'),str(obj),'-o',str(binary)]]
        for command in commands:subprocess.run(command,check=True,timeout=120)
        for label,path in images.items():
            result=subprocess.run([str(binary),str(path)],capture_output=True,text=True,check=True,timeout=30,
                                  env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
            observed=json.loads(result.stdout)
            require(observed['data_allocation_request_bytes']==records[label]['section_loader']['data_allocation_request_bytes'],
                    'Python allocation model differs from pinned production helper')
            (out/f'{label}-{int(sanitized)}.json').write_text(result.stdout)
            runs.append({'image':label,'sanitized':sanitized,'commands':commands,'result':observed})
        obj.unlink();binary.unlink()
    delta=records['native']['file_size_bytes']-RETAINED_SIZE
    comparison={'retained_version':'1.3.25','retained_size_bytes':RETAINED_SIZE,'retained_sha256':RETAINED_SHA256,
                'retained_elf_independently_checked':bool(args.retained_elf),'file_growth_bytes':delta,
                'file_growth_percent':100*delta/RETAINED_SIZE}
    if args.retained_elf:
        before=records['retained059']['section_loader']['resident_section_allocation_request_bytes']
        after=records['native']['section_loader']['resident_section_allocation_request_bytes']
        comparison.update(resident_section_growth_bytes=after-before,resident_section_growth_percent=100*(after-before)/before)
        old={row['name']:row['size_bytes'] for row in records['retained059']['sections']}
        new={row['name']:row['size_bytes'] for row in records['native']['sections']}
        comparison['section_size_deltas']={name:new.get(name,0)-old.get(name,0) for name in sorted(set(old)|set(new))}
    report={'schema':1,'purpose':'standalone-development-software-qualification-only','runtime_commit':RUNTIME_PIN,
        'runtime_source_sha256':hashes,'reporter_sha256':sha(Path(__file__).read_bytes()),
        'admission_harness_sha256':sha(ADMISSION_SOURCE.encode()),'images':records,'retained059_comparison':comparison,
        'admission_runs':runs,'admission_scope':'Pinned production structural validator, ordinary import/role/lifecycle helper and alignment-reservation/placement helpers; no relocation, Xtensa execution, native grant binding or device heap test',
        'dynamic_sleep_snapshot_bytes':'2 * native_width * native_height, separate from loaded sections',
        'linked_device_memory_fit_qualified':False,
        'unqualified_requirements':['selected linked-loader configuration','allocator capabilities and fragmentation',
            'concurrent Runtime/provider/cached-image memory','app stacks and dynamic allocations','frame/Quick Controls/sleep buffers',
            'linked public import implementations and ABI']}
    (out/'elf-memory-admission.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Pinned ordinary-app admission and allocation reporting passed; linked-device memory fit remains unqualified')


ADMISSION_SOURCE = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "runtime/packages/ElfAdmission.h"
#include "private/esp_elf_data_layout.h"
extern "C" bool esp_elf_validate_file(const uint8_t*,size_t);
enum class Role {Application, Driver, Any};
static unsigned yields,imports;
static bool safe_state=true,reject_imports;
static bool safe(){return safe_state;}
static void yield(){++yields;}
static uint32_t now(){return yields;}
static bool import(const char*name){++imports;return !reject_imports&&RiscPackages::ordinaryImport(name);}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long length=ftell(f);assert(length>0);rewind(f);
 std::vector<uint8_t> data(length);assert(fread(data.data(),1,data.size(),f)==data.size());fclose(f);
 const RiscPackages::ElfAdmissionChecks checks={esp_elf_validate_file,import,safe,yield,now};
 assert(RiscPackages::admitElf(data.data(),data.size(),Role::Application,false,checks));
 const unsigned admitted_yields=yields,admitted_imports=imports;
 reject_imports=true;assert(!RiscPackages::admitElf(data.data(),data.size(),Role::Application,false,checks));reject_imports=false;
 safe_state=false;assert(!RiscPackages::admitElf(data.data(),data.size(),Role::Application,false,checks));safe_state=true;
 assert(!RiscPackages::admitElf(data.data(),data.size(),Role::Driver,false,checks));
 const auto*h=reinterpret_cast<const elf32_hdr_t*>(data.data());const auto*s=reinterpret_cast<const elf32_shdr_t*>(data.data()+h->shoff);
 const char*names=reinterpret_cast<const char*>(data.data()+s[h->shstrndx].offset);
 const char*wanted[]={".data",".rodata",".data.rel.ro",".bss"};const elf32_shdr_t*sections[4]={};uint32_t capacity=0;
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<h->shnum;++j)if(!strcmp(wanted[i],names+s[j].name))sections[i]=&s[j];
 for(const auto*sec:sections)if(sec&&sec->size)assert(esp_elf_data_reserve(sec->size,sec->addralign,sec->addr,&capacity));
 unsigned placements=0;
 for(uintptr_t residue=0;residue<16;++residue){uint32_t cursor=0;for(const auto*sec:sections){if(!sec||!sec->size)continue;uint32_t offset;
  assert(esp_elf_data_place(0x10000+residue,cursor,capacity,sec->size,sec->addralign,sec->addr,&offset));
  assert(((0x10000+residue+offset)-sec->addr)%((sec->addralign<4)?4:sec->addralign)==0);cursor=offset+sec->size;++placements;
 }assert(cursor<=capacity);}
 printf("{\"ordinary_application_admitted\":true,\"missing_import_rejected\":true,\"unsafe_state_rejected\":true,\"driver_role_rejected\":true,\"cooperative_yields\":%u,\"import_checks\":%u,\"data_allocation_request_bytes\":%u,\"alignment_placements_checked\":%u,\"linked_device_memory_fit_qualified\":false}\n",admitted_yields,admitted_imports,capacity,placements);
}
'''

if __name__ == '__main__':
    main()
