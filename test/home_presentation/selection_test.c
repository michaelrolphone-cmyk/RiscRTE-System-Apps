#include "PortableHomeSelection.h"
#include "RiscProviderV2.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t record[4];static uint32_t length=4;static int status;
static int get(void*c,const char*k,void*out,uint32_t cap,uint32_t*count){(void)c;assert(!strcmp(k,PORTABLE_HOME_VIEW_KEY)&&cap==4);memcpy(out,record,4);*count=length;return status;}
int main(void){
 const risc_driver_v2*d=t5_driver_get(2);assert(d&&d->start(NULL,0));
 const risc_home_presentation_v1*p=d->capability;assert(risc_home_presentation_get_v1(p,sizeof(*p))==p);
 assert(!risc_home_presentation_get_v1(p,sizeof(*p)-1));
 risc_home_presentation_v1 changed=*p;changed.tag=0;assert(!risc_home_presentation_get_v1(&changed,sizeof(changed)));
 changed=*p;changed.crash_namespace=0;assert(!risc_home_presentation_get_v1(&changed,sizeof(changed)));
 const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,NULL};unsigned saved=99;
 for(unsigned view=1;view<=2;view++){assert(portable_home_view_encode(view,record));assert(portable_home_view_load(&kv,&saved)==0&&saved==view);assert(portable_home_view_choose(p,0,saved)==view);}
 saved=99;status=RISC_KEY_VALUE_NOT_FOUND;assert(portable_home_view_load(&kv,&saved)==1&&saved==99);assert(portable_home_view_choose(p,1,saved)==p->default_view);
 status=RISC_KEY_VALUE_OK;record[3]^=1;assert(portable_home_view_load(&kv,&saved)==-1&&saved==99);
 length=3;assert(portable_home_view_load(&kv,&saved)==-1);
 status=RISC_KEY_VALUE_BUFFER_SMALL;assert(portable_home_view_load(&kv,&saved)==-2);
 assert(portable_home_view_load(NULL,&saved)==-2&&!portable_home_view_choose(NULL,1,0));
 assert(d->quiesce());d->stop();
 puts("Home semantic default/user override and arbitrary data binding: no device/geometry discriminator, no preference writes PASS");
}
