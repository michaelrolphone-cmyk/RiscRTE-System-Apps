#include "InstalledCatalog.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static installed_record app(const char*id,uint8_t kind,uint8_t medium){
 installed_record r={0};strcpy(r.id,id);strcpy(r.name,id);strcpy(r.version,"1.0.0");
 snprintf(r.path,sizeof(r.path),"Apps/%s/%s.elf",id,id);r.kind=kind;r.medium=medium;r.installation_revision=1;r.admitted=true;return r;
}
int main(void){
 installed_catalog*c=calloc(1,sizeof(*c)),*restored=calloc(1,sizeof(*c));assert(c&&restored);
 installed_catalog_init(c);installed_catalog_init(restored);
 installed_record a=app("new-app",INSTALLED_APP,INSTALLED_SD),d=app("driver",INSTALLED_DRIVER,INSTALLED_INTERNAL),out;
 assert(installed_catalog_begin(c,INSTALLED_INTERNAL));assert(installed_catalog_offer(c,&d));assert(installed_catalog_finish(c,true));
 assert(installed_catalog_begin(c,INSTALLED_SD));assert(installed_catalog_offer(c,&a));assert(c->count==1);assert(installed_catalog_finish(c,true));assert(c->count==2);
 installed_selection s;assert(!installed_catalog_select(c,1,&s));installed_catalog_media(c,true);assert(installed_catalog_select(c,1,&s));assert(installed_catalog_resolve(c,&s,&out));assert(!strcmp(out.id,a.id));
 // An I/O failure or duplicate cannot remove or partially replace old rows.
 uint32_t generation=c->generation;assert(installed_catalog_begin(c,INSTALLED_SD));assert(!installed_catalog_finish(c,false));assert(c->count==2&&c->generation==generation);
 assert(installed_catalog_begin(c,INSTALLED_SD));assert(installed_catalog_offer(c,&a));assert(!installed_catalog_offer(c,&a));assert(!installed_catalog_finish(c,true));assert(c->count==2&&c->generation==generation);
 // Update publishes atomically and invalidates touch tokens.
 ++a.installation_revision;strcpy(a.version,"1.1.0");assert(installed_catalog_begin(c,INSTALLED_SD));assert(installed_catalog_offer(c,&a));assert(installed_catalog_finish(c,true));assert(!installed_catalog_resolve(c,&s,&out));
 unsigned char*cache=malloc(INSTALLED_CATALOG_CACHE_MAX);assert(cache);size_t n=installed_catalog_encode(c,"cohort/policy-v1",cache,INSTALLED_CATALOG_CACHE_MAX);assert(n>20);
 assert(installed_catalog_decode(restored,"cohort/policy-v1",cache,n));assert(restored->count==2&&!restored->sd_ready&&!restored->current[1].admitted);assert(!installed_catalog_select(restored,1,&s));
 assert(!installed_catalog_decode(restored,"another-policy",cache,n));assert(restored->count==2);
 for(size_t i=0;i<n;++i){cache[i]^=1;assert(!installed_catalog_decode(restored,"cohort/policy-v1",cache,n));cache[i]^=1;}
 for(size_t i=0;i<n;++i)assert(!installed_catalog_decode(restored,"cohort/policy-v1",cache,i));
 assert(restored->count==2);
 // No media retains display metadata; a successful empty scan removes SD only.
 installed_catalog_media(c,false);assert(c->count==2);assert(installed_catalog_begin(c,INSTALLED_SD));assert(installed_catalog_finish(c,true));assert(c->count==1&&c->current[0].kind==INSTALLED_DRIVER);
 assert(installed_catalog_begin(c,INSTALLED_SD));strcpy(a.path,"Apps/../escape.elf");assert(!installed_catalog_offer(c,&a));assert(!installed_catalog_finish(c,true));assert(c->count==1);
 // Capacity failure is atomic; exactly the maximum catalog round-trips.
 assert(installed_catalog_begin(c,INSTALLED_SD));
 for(unsigned i=0;i<INSTALLED_CATALOG_MAX-1;++i){char id[32];snprintf(id,sizeof(id),"app-%u",i);a=app(id,INSTALLED_APP,INSTALLED_SD);assert(installed_catalog_offer(c,&a));}
 a=app("overflow",INSTALLED_APP,INSTALLED_SD);assert(!installed_catalog_offer(c,&a));assert(!installed_catalog_finish(c,true));assert(c->count==1);
 assert(installed_catalog_begin(c,INSTALLED_SD));
 for(unsigned i=0;i<INSTALLED_CATALOG_MAX-1;++i){char id[32];snprintf(id,sizeof(id),"app-%u",i);a=app(id,INSTALLED_APP,INSTALLED_SD);assert(installed_catalog_offer(c,&a));}
 assert(installed_catalog_finish(c,true));assert(c->count==INSTALLED_CATALOG_MAX);
 n=installed_catalog_encode(c,"cohort/policy-v1",cache,INSTALLED_CATALOG_CACHE_MAX);assert(n);
 assert(installed_catalog_decode(restored,"cohort/policy-v1",cache,n));assert(restored->count==INSTALLED_CATALOG_MAX);
 c->generation=UINT32_MAX;assert(!installed_catalog_begin(c,INSTALLED_SD));assert(!installed_catalog_select(c,0,&s));
 free(cache);free(restored);free(c);puts("installed catalog: transaction, media, update, selection, cache corruption and truncation PASS");
}
