#include "frame.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void){
 frame_spec s={800,480,100,48000,FRAME_FORMAT,"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};
 assert(frame_valid(&s));s.width=799;assert(!frame_valid(&s));s.width=800;s.stride=101;assert(!frame_valid(&s));s.stride=100;s.bytes=47999;assert(!frame_valid(&s));s.bytes=48000;s.format="gray4";assert(!frame_valid(&s));s.format=FRAME_FORMAT;s.sha256="invalid";assert(!frame_valid(&s));
 uint8_t *out=calloc(FRAME_BYTES,1),*in=malloc(FRAME_BYTES);for(int i=0;i<FRAME_BYTES;i++)in[i]=(uint8_t)(i*19+1);size_t n=0;
 assert(frame_add(&n,out,in,47999));assert(n==47999);assert(!frame_add(&n,out,in,2));assert(n==47999);assert(frame_add(&n,out,in+47999,1));assert(n==48000);assert(!memcmp(out,in,48000));assert(!frame_add(&n,out,in,1));
 assert(frame_path("/v1/artifacts/artifact_abc/bytes?grant=transfer_123","artifact_abc"));
 assert(!frame_path("https://evil.test/a","artifact_abc"));assert(!frame_path("//evil.test/a","artifact_abc"));assert(!frame_path("/v1/artifacts/artifact_other/bytes?grant=x","artifact_abc"));assert(!frame_path("/v1/artifacts/artifact_abc/bytes?grant=x&url=evil","artifact_abc"));
 assert(frame_can_render(1,30002,true,true));assert(!frame_can_render(1,30001,true,true));assert(!frame_can_render(1,50000,false,true));assert(!frame_can_render(1,50000,true,false));assert(!frame_can_render(50000,0,true,true));
 free(out);free(in);puts("frame validation, bounds, path confinement and expiry tests passed");
}
