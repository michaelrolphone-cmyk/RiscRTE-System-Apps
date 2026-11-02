#include "frame.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
bool frame_valid(const frame_spec *s) {
 if(!s || s->width!=800 || s->height!=480 || s->stride!=100 || s->bytes!=48000 || !s->format || strcmp(s->format,FRAME_FORMAT) || !s->sha256 || strlen(s->sha256)!=64) return false;
 for(unsigned i=0;i<64;i++) if(!strchr("0123456789abcdef",s->sha256[i])) return false;
 return true;
}
bool frame_add(size_t *received,uint8_t *dst,const uint8_t *src,size_t length) {
 if(!received || !dst || !src || *received>FRAME_BYTES || length>FRAME_BYTES-*received) return false;
 memcpy(dst+*received,src,length); *received+=length; return true;
}
bool frame_path(const char *path,const char *id) {
 if(!path || !id || strncmp(id,"artifact_",9) || strlen(id)>100) return false;
 for(const char *p=id;*p;p++) if(!isalnum((unsigned char)*p)&&*p!='_') return false;
 char prefix[140]; snprintf(prefix,sizeof prefix,"/v1/artifacts/%s/bytes?grant=",id);
 size_t n=strlen(prefix); if(strncmp(path,prefix,n)||!path[n]||strlen(path+n)>100) return false;
 for(const char *p=path+n;*p;p++) if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-') return false;
 return true;
}
bool frame_can_render(int64_t now,int64_t deadline,bool healthy,bool complete) {
 /* Power on + refresh + power off are bounded to <30 s. Do not start near expiry. */
 return healthy && complete && deadline-now>30000;
}

bool frame_cursor_new(double sequence,int64_t cursor) {
 return sequence>0 && sequence>cursor && sequence<=9007199254740991.0 && sequence==(double)(int64_t)sequence;
}
