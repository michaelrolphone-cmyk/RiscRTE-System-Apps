#!/usr/bin/env python3
"""Compare actual old/new fill, full buffers and checkpoint-failure prefixes."""
from pathlib import Path
import argparse,json,os,subprocess

def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
 s=(r/'lib/PortableApps/src/adapter.c').read_text();s=s.replace('static void native_point(int *x,int *y) {','static void native_point(int *x,int *y) { ++transforms;',1)
 start=s.index('static bool raster_checkpoint(void) {');brace=s.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 s=s[:start]+'static bool raster_checkpoint(void) {++checks;if(fail_at&&checks==fail_at){failed=true;return false;}return true;}'+s[end:]
 start=s.index('static void fill(');brace=s.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 reference=s[start:end].replace('static void fill(', 'static void reference_fill(',1).replace('#if !defined(PORTABLE_CONTEXTS_CLIENT) /* RGB565 affine span default; legacy flag is compatible. */', '#if 0')
 s=s[:end]+'\n'+reference+'\n'+s[end:]
 (o/'adapter-counted.c').write_text(s)
 fixture=r'''#include <stdint.h>
#include <stdio.h>
#include <assert.h>
static unsigned transforms,checks,fail_at;
#include "adapter-counted.c"
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};const unsigned portable_catalog_count=0;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){(void)v;return NULL;}
static uint8_t pixels[502*1016+32],expected[502*1016+32];
int main(void){
 uint64_t hash=1469598103934665603ULL,total_transforms=0,total_checks=0,reference_transforms=0;unsigned cases=0;
 const uint16_t colors[]={0,0xffff,0x9753};
 const int rects[][4]={{0,0,466,466},{-7,-9,35,47},{465,465,17,19},{3,5,79,61},{31,29,1,1},{0,0,0,17},{900,900,2,2}};
 const unsigned stops[]={0,1,2,17,425},offsets[]={0,510,UINT32_MAX-10};
 for(unsigned shape=0;shape<2;shape++)for(unsigned rotation=0;rotation<4;rotation++)
 for(unsigned rect=0;rect<7;rect++)for(unsigned color=0;color<3;color++)
 for(unsigned stop=0;stop<5;stop++)for(unsigned offset=0;offset<3;offset++){
  memset(pixels,0xa5,sizeof(pixels));failed=false;checks=transforms=0;fail_at=stops[stop];raster_pixels=offsets[offset];
  info.width=466;info.height=shape?502:466;surface_format=RISC_DISPLAY_FORMAT_RGB565;
  paper_rotated=rotation&1;paper_flip_ui=rotation>>1;
  surface=(risc_display_surface_v1){.frame=1,.pixels=pixels+16,.width=info.width,.height=info.height,.stride_bytes=shape?1016:932,.size_bytes=(shape?1016:932)*info.height,.pixel_format=surface_format};
  reference_fill(rects[rect][0],rects[rect][1],rects[rect][2],rects[rect][3],colors[color]);
  memcpy(expected,pixels,sizeof(pixels));unsigned reference_checks=checks,reference_pixels=raster_pixels;bool reference_failed=failed;reference_transforms+=transforms;
  memset(pixels,0xa5,sizeof(pixels));failed=false;checks=transforms=0;raster_pixels=offsets[offset];
  fill(rects[rect][0],rects[rect][1],rects[rect][2],rects[rect][3],colors[color]);
  assert(!memcmp(expected,pixels,sizeof(pixels)));assert(checks==reference_checks&&raster_pixels==reference_pixels&&failed==reference_failed);
  for(unsigned i=0;i<sizeof(pixels);i++){hash^=pixels[i];hash*=1099511628211ULL;}
  hash^=raster_pixels;hash*=1099511628211ULL;hash^=checks;hash*=1099511628211ULL;hash^=failed;hash*=1099511628211ULL;
  for(unsigned i=0;i<16;i++)assert(pixels[i]==0xa5);
  for(unsigned y=0;y<info.height;y++)for(unsigned x=info.width*2;x<surface.stride_bytes;x++)assert(pixels[16+y*surface.stride_bytes+x]==0xa5);
  for(size_t i=16+surface.size_bytes;i<sizeof(pixels);i++)assert(pixels[i]==0xa5);
  total_transforms+=transforms;total_checks+=checks;cases++;
 }
 printf("{\"cases\":%u,\"buffer_and_fault_prefix_hash\":\"%016llx\",\"transforms\":%llu,\"reference_transforms\":%llu,\"checkpoints\":%llu}\n",cases,(unsigned long long)hash,(unsigned long long)total_transforms,(unsigned long long)reference_transforms,(unsigned long long)total_checks);
}
'''
 (o/'fixture.c').write_text(fixture)
 base=[os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror','-DPORTABLE_PAPER_PREFERENCES','-I'+str(o),'-I'+str(r/'lib/PortableApps/src'),'-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
 if a.sanitize:base+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie']
 results={}
 for name,extra in [('default',[]),('legacy-marker',['-DPORTABLE_RGB565_SPAN_FILL'])]:
  exe=o/name;subprocess.run([*base,*extra,str(o/'fixture.c'),'-o',str(exe)],check=True);results[name]=json.loads(subprocess.check_output([str(exe)],text=True,timeout=120))
 assert results['default']['buffer_and_fault_prefix_hash']==results['legacy-marker']['buffer_and_fault_prefix_hash']
 assert results['default']['checkpoints']==results['legacy-marker']['checkpoints']
 assert results['legacy-marker']['transforms']==results['default']['transforms']
 assert results['default']['transforms']<results['default']['reference_transforms']
 (o/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results))
if __name__=='__main__':main()
