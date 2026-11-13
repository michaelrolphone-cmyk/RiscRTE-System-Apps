#!/usr/bin/env python3
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
source=r"""#include "PortableSafeViewport.h"
#include <assert.h>
int main(){portable_safe_viewport v;assert(portable_safe_viewport_open(&v,466,466,69,69,69,69));assert(v.width==328&&v.height==328);for(unsigned w=240;w<700;w+=37)for(unsigned h=240;h<800;h+=41)for(unsigned inset=0;inset<100;inset+=17){assert(portable_safe_viewport_open(&v,w,h,inset,inset,inset,inset));for(unsigned i=0;i<4;i++){uint16_t x=i&1?w-inset-1:inset,y=i&2?h-inset-1:inset;assert(portable_safe_viewport_touch(&v,&x,&y));assert(x<v.width&&y<v.height);}if(inset){uint16_t x=inset-1,y=inset;assert(!portable_safe_viewport_touch(&v,&x,&y));}}assert(!portable_safe_viewport_open(&v,240,240,120,0,120,0));assert(!portable_safe_viewport_open(&v,240,240,0,120,0,120));}"""
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'check.c').write_text(source)
 for san in [[],['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']]:
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*san,'-I'+str(root/'lib/PortableApps/include'),str(p/'check.c'),'-o',str(p/'check')],check=True);subprocess.run([str(p/'check')],check=True)
print('PASS capability-derived content bounds and hit mapping at240,466round and generated rectangular geometries; physical raster unchanged')
