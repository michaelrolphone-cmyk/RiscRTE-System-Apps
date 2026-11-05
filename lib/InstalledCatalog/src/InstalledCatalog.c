#include "InstalledCatalog.h"
#include <string.h>
static bool text(const char*s,size_t cap,bool empty) {
  size_t n=0;while(n<cap && s[n])++n;
  if(n==cap || (!empty && !n))return false;
  const char*end=s+n;
  for(;s<end;++s)if((unsigned char)*s<32 || (unsigned char)*s==127)return false;
  return true;
}
static bool record_ok(const installed_record*r) {
  if(!r || !text(r->id,sizeof(r->id),false) || !text(r->version,sizeof(r->version),false) ||
     !text(r->path,sizeof(r->path),false) || !text(r->name,sizeof(r->name),false) ||
     !text(r->icon,sizeof(r->icon),true) || !r->installation_revision ||
     (r->kind!=INSTALLED_APP && r->kind!=INSTALLED_DRIVER) ||
     (r->medium!=INSTALLED_SD && r->medium!=INSTALLED_INTERNAL))return false;
  for(const char*p=r->id;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_'||*p=='.'))return false;
  if(r->path[0]=='/' || strchr(r->path,'\\'))return false;
  const char*p=r->path;
  while(*p){const char*end=strchr(p,'/');size_t n=end?(size_t)(end-p):strlen(p);
    if(!n || (n==1&&*p=='.') || (n==2&&p[0]=='.'&&p[1]=='.'))return false;
    if(!end)break;
    p=end+1;if(!*p)return false;
  }
  return true;
}
static bool generation(installed_catalog*c){if(c->generation==UINT32_MAX)return false;++c->generation;return true;}
void installed_catalog_init(installed_catalog*c){if(c){memset(c,0,sizeof(*c));c->generation=1;}}
bool installed_catalog_begin(installed_catalog*c,uint8_t medium) {
  if(!c || c->scanning || c->generation==UINT32_MAX ||
     (medium!=INSTALLED_INTERNAL && medium!=INSTALLED_SD))return false;
  c->scanning=true;c->failed=false;c->scanning_medium=medium;c->pending_count=0;
  for(uint32_t i=0;i<c->count;++i)if(c->current[i].medium!=medium)c->pending[c->pending_count++]=c->current[i];
  return true;
}
bool installed_catalog_begin_all(installed_catalog*c){
 if(!c||c->scanning||c->generation==UINT32_MAX)return false;
 c->scanning=true;c->failed=false;c->scanning_medium=0;c->pending_count=0;return true;
}
bool installed_catalog_offer(installed_catalog*c,const installed_record*r) {
  if(!c || !c->scanning || c->failed)return false;
  if(!record_ok(r) || (c->scanning_medium&&r->medium!=c->scanning_medium) || c->pending_count==INSTALLED_CATALOG_MAX){c->failed=true;return false;}
  for(uint32_t i=0;i<c->pending_count;++i)if((c->pending[i].medium==r->medium && !strcmp(c->pending[i].id,r->id)) ||
      (c->pending[i].medium==r->medium && !strcmp(c->pending[i].path,r->path))){c->failed=true;return false;}
  c->pending[c->pending_count++]=*r;return true;
}
bool installed_catalog_finish(installed_catalog*c,bool complete) {
  if(!c || !c->scanning)return false;
  bool publish=complete&&!c->failed&&generation(c);
  if(publish){memcpy(c->current,c->pending,c->pending_count*sizeof(*c->current));c->count=c->pending_count;}
  c->scanning=false;c->pending_count=0;c->failed=false;return publish;
}
void installed_catalog_media(installed_catalog*c,bool ready){if(c && c->sd_ready!=ready){c->sd_ready=ready;(void)generation(c);}}
static const installed_record* preferred(const installed_catalog*c,const char*id){
 const installed_record*internal=0,*sd=0;
 for(uint32_t i=0;i<c->count;++i)if(!strcmp(c->current[i].id,id)){
  if(c->current[i].medium==INSTALLED_SD)sd=&c->current[i];else internal=&c->current[i];
 }
 return c->sd_ready&&sd&&sd->admitted?sd:internal?internal:sd;
}
bool installed_catalog_select(const installed_catalog*c,uint32_t i,installed_selection*s) {
  if(!c || !s || c->generation==UINT32_MAX || i>=c->count || c->current[i].kind!=INSTALLED_APP ||
     !preferred(c,c->current[i].id)->admitted || (preferred(c,c->current[i].id)->medium==INSTALLED_SD&&!c->sd_ready))return false;
  s->generation=c->generation;strcpy(s->id,c->current[i].id);return true;
}
bool installed_catalog_resolve(const installed_catalog*c,const installed_selection*s,installed_record*out) {
  if(!c || !s || !out || !text(s->id,sizeof(s->id),false) || s->generation!=c->generation)return false;
  for(uint32_t i=0;i<c->count;++i)if(!strcmp(c->current[i].id,s->id)){
    installed_selection check;if(!installed_catalog_select(c,i,&check))return false;*out=*preferred(c,c->current[i].id);return true;
  }return false;
}
/* Explicit wire layout: no struct padding, native endianness or pointer data. */
static uint32_t checksum(const uint8_t*p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h;}
static void put32(uint8_t*p,uint32_t x){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(x>>(8*i));}
static uint32_t get32(const uint8_t*p){uint32_t x=0;for(unsigned i=0;i<4;++i)x|=(uint32_t)p[i]<<(8*i);return x;}
static bool write_text(uint8_t*p,size_t cap,size_t*at,const char*s){size_t n=strlen(s);if(n>255 || *at>=cap || n>cap-*at-1)return false;p[(*at)++]=(uint8_t)n;memcpy(p+*at,s,n);*at+=n;return true;}
static bool read_text(const uint8_t*p,size_t cap,size_t*at,char*s,size_t bound){if(*at>=cap)return false;size_t n=p[(*at)++];if(n>=bound||n>cap-*at||memchr(p+*at,0,n))return false;memcpy(s,p+*at,n);s[n]=0;*at+=n;return true;}
size_t installed_catalog_encode(const installed_catalog*c,const char*key,void*bytes,size_t cap) {
  if(!c||!key||!text(key,97,false)||!bytes||cap<18||c->count>INSTALLED_CATALOG_MAX)return 0;
  uint8_t*p=bytes;memcpy(p,"ICAT\1\0\0\0",8);put32(p+8,c->count);size_t at=12;
  if(!write_text(p,cap-4,&at,key))return 0;
  for(uint32_t i=0;i<c->count;++i){const installed_record*r=&c->current[i];
    if(!record_ok(r)||at>cap-4||cap-4-at<6)return 0;
    p[at++]=r->kind;p[at++]=r->medium;put32(p+at,r->installation_revision);at+=4;
    if(!write_text(p,cap-4,&at,r->id)||!write_text(p,cap-4,&at,r->version)||!write_text(p,cap-4,&at,r->path)||!write_text(p,cap-4,&at,r->name)||!write_text(p,cap-4,&at,r->icon))return 0;
  }
  put32(p+at,checksum(p,at));return at+4;
}
bool installed_catalog_decode(installed_catalog*c,const char*key,const void*bytes,size_t size) {
  if(!c||c->scanning||!key||!text(key,97,false)||!bytes||size<18||size>INSTALLED_CATALOG_CACHE_MAX||c->generation==UINT32_MAX)return false;
  const uint8_t*p=bytes;uint32_t count=get32(p+8);
  if(memcmp(p,"ICAT\1\0\0\0",8)||count>INSTALLED_CATALOG_MAX||get32(p+size-4)!=checksum(p,size-4))return false;
  char stored[97];size_t at=12;if(!read_text(p,size-4,&at,stored,sizeof(stored))||strcmp(key,stored))return false;
  for(uint32_t i=0;i<count;++i){installed_record*r=&c->pending[i];memset(r,0,sizeof(*r));
    if(at>size-4||size-4-at<6)return false;
    r->kind=p[at++];r->medium=p[at++];r->installation_revision=get32(p+at);at+=4;
    if(!read_text(p,size-4,&at,r->id,sizeof(r->id))||!read_text(p,size-4,&at,r->version,sizeof(r->version))||!read_text(p,size-4,&at,r->path,sizeof(r->path))||!read_text(p,size-4,&at,r->name,sizeof(r->name))||!read_text(p,size-4,&at,r->icon,sizeof(r->icon))||!record_ok(r))return false;
    for(uint32_t j=0;j<i;++j)if((c->pending[j].medium==r->medium&&!strcmp(c->pending[j].id,r->id))||(c->pending[j].medium==r->medium&&!strcmp(c->pending[j].path,r->path)))return false;
  }
  if(at!=size-4)return false;
  memcpy(c->current,c->pending,count*sizeof(*c->current));c->count=count;c->sd_ready=false;return generation(c);
}
