#!/usr/bin/env python3
"""Execute the selected Runtime's exact loader allocation/section functions.

Offline fault injection only. No execution of Xtensa code, devices or firmware
writes. Requires a local coordinated Runtime checkout and built provider ELFs.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def extract(text,signature):
 start=text.index(signature);opening=text.index('{',start);depth=0
 for pos in range(opening,len(text)):
  if text[pos]=='{':depth+=1
  elif text[pos]=='}':
   depth-=1
   if not depth:return text[start:pos+1]
 raise ValueError('Incomplete loader function')
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--artifacts',type=Path,default=ROOT/'dist/portable/updates');args=p.parse_args()
runtime=args.runtime.resolve();out=ROOT/'build/portable/loader-memory';out.mkdir(parents=True,exist_ok=True)
adapter=runtime/'lib/elf_loader/src/esp_elf_adapter.c';loader=runtime/'lib/elf_loader/src/esp_elf.c';config=(runtime/'platformio.ini').read_text()
for flag in ['CONFIG_ELF_LOADER_LOAD_PSRAM=1','CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1']:
 if flag not in config:raise ValueError('Deployment lacks required no-fallback PSRAM loader config: '+flag)
allocation=extract(adapter.read_text(),'void *esp_elf_malloc(');sections=extract(loader.read_text(),'static int esp_elf_load_section(')
(out/'sdkconfig.h').write_text('#define CONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR 1\n#define CONFIG_ELF_LOADER_LOAD_PSRAM 1\n')
source=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "private/elf_types.h"
#include "private/esp_elf_data_layout.h"
#define MALLOC_CAP_SPIRAM 4u
#define MALLOC_CAP_8BIT 8u
#define ESP_IDF_VERSION 40407
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define stype(s,t) ((s)->type==(t))
#define sflags(s,f) (((s)->flags&(f))==(f))
static unsigned calls,fail_at,live;
static uint32_t requested[3];
static void* heap_caps_malloc(uint32_t n,uint32_t caps){
 assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));assert(calls<3);requested[calls++]=n;
 if(calls==fail_at)return NULL;void*p=malloc(n);assert(p);++live;return p;
}
static void esp_elf_free(void*p){if(p){assert(live);--live;free(p);}}
'''+allocation+'\n'+sections+r'''
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);rewind(f);uint8_t*bytes=malloc((size_t)n);assert(bytes);assert(fread(bytes,1,(size_t)n,f)==(size_t)n);fclose(f);
 for(unsigned scenario=1;scenario<=3;++scenario){
  calls=live=0;fail_at=scenario<3?scenario:0;esp_elf_t elf={0};int result=esp_elf_load_section(&elf,bytes);
  if(fail_at){assert(result==-ENOMEM);assert(calls==fail_at);assert(!live&&!elf.entry&&!elf.ptext&&!elf.pdata);}
  else {assert(result==0&&calls==2&&live==2);assert(elf.sec[ELF_SEC_BSS].size>=512u*1024u);assert(requested[1]<=704u*1024u);esp_elf_free(elf.pdata);esp_elf_free(elf.ptext);assert(!live);}
 }
 free(bytes);puts("Exact Runtime loader: PSRAM-only code/data, bounded provider BSS, text/data OOM returns ENOMEM before entry passed");return 0;
}
'''
(out/'loader.c').write_text(source);binary=out/'loader'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-parameter','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-I'+str(out),'-I'+str(runtime/'lib/elf_loader/include'),str(out/'loader.c'),'-o',str(binary)],check=True)
for name in ('software-update-firmware','software-update-apps'):subprocess.run([str(binary),str(args.artifacts/name/'driver.elf')],check=True)
record={'scope':'offline exact loader function execution with mock PSRAM allocator; no Xtensa code/device execution','runtime_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip(),'runtime_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=runtime,text=True).strip()),'source_sha256':{str(p.relative_to(runtime)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [adapter,loader,runtime/'platformio.ini']},'provider_bss_limit':704*1024,'checks':['exact PSRAM flags; no internal fallback','text allocation failure before entry','data allocation failure frees text and clears entry','both target providers map BSS within bound']}
(out/'evidence.json').write_text(json.dumps(record,indent=2)+'\n')
