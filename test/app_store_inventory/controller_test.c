#include "app_store_inventory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static risc_runtime_api_v1 rt,other;static const risc_runtime_api_v1*current;
static installed_apps_feed_v1 feed;static software_update_v1 update;static risc_storage_volume_api_v1 volume;
static app_store_inventory state;static installed_app_v1 row;
static int retained,acquires,releases,volume_acquires,submits,cancels,polls,feed_steps,feed_cancels,update_steps,begins,ready_calls;
static int absent,dirty,release_bad,mutate_get,replace_on_submit,bad_get,rc_submit,rc_cancel,rc_step,rc_feed,rc_ready;
static uint32_t generation,feed_generation,n_installed,n_staged,n_feed;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return current;}
static bool alive(void*c){(void)c;return !retained;}
static void keep(void*c){(void)c;retained++;}
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!retained&&v==1&&id==0);acquires++;if(absent&&!strcmp(n,INSTALLED_APPS_CAPABILITY)){if(dirty)g->slot=9;return false;}g->slot=acquires;g->generation=1;g->api=!strcmp(n,INSTALLED_APPS_CAPABILITY)?(const void*)&feed:(const void*)&volume;if(!strcmp(n,"storage.volume"))volume_acquires++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(!retained);releases++;if(release_bad)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static int refresh(void*c){(void)c;return 0;}
static uint32_t gen(void*c){(void)c;assert(!retained);return generation;}
static uint32_t fgen(void*c){(void)c;assert(!retained);return feed_generation;}
static uint32_t count(void*c){(void)c;return n_installed;}
static uint32_t staged(void*c){(void)c;return n_staged;}
static uint32_t fcount(void*c){(void)c;return n_feed;}
static bool get(void*c,uint32_t i,installed_app_v1*out){(void)c;(void)i;assert(!retained);*out=row;if(bad_get)memset(out->id,'x',sizeof(out->id));if(mutate_get)generation++;return true;}
static int step(void*c,const risc_storage_volume_api_v1*v){(void)c;assert(!retained&&v==&volume);polls++;return rc_step;}
static int cancel(void*c){(void)c;assert(!retained);cancels++;return rc_cancel;}
static int remove_app(void*c,const char*id){(void)c;assert(!retained&&!strcmp(id,row.id));submits++;if(replace_on_submit)current=&other;return rc_submit;}
static int install(void*c,const char*id,const char*version){assert(!strcmp(version,row.version));return remove_app(c,id);}
static int netready(void*c,uint64_t utc){(void)c;assert(utc==1234);ready_calls++;return rc_ready;}
static int provision(void*c,const char*j){(void)c;(void)j;return 0;}
static int frefresh(void*c,uint64_t utc){(void)c;assert(utc==1234);return rc_feed;}
static int fstep(void*c){(void)c;feed_steps++;return rc_feed;}
static int fcancel(void*c){(void)c;feed_cancels++;return rc_cancel;}
static int finstall(void*c,uint32_t i,uint32_t g){assert(i==0&&g==feed_generation);return remove_app(c,row.id);}
static bool yes(void*c){(void)c;assert(!retained);return true;}
static bool urefresh(void*c,uint64_t utc){(void)c;(void)utc;return true;}
static bool ustep(void*c){(void)c;update_steps++;return true;}
static bool status(void*c,software_update_status_v1*out){(void)c;assert(!state.draining);*out=(software_update_status_v1){.struct_size=sizeof(*out),.count=1,.state=SOFTWARE_UPDATE_LIST};return true;}
static bool uget(void*c,uint32_t i,software_update_row_v1*out){(void)c;assert(i==0);*out=(software_update_row_v1){.struct_size=sizeof(*out)};strcpy(out->id,"release");strcpy(out->version,"1");return true;}
static bool begin(void*c,uint32_t i,uint64_t utc){(void)c;(void)utc;assert(i==0);begins++;return true;}
static void reset(void){
 retained=acquires=releases=volume_acquires=submits=cancels=polls=feed_steps=feed_cancels=update_steps=begins=ready_calls=0;
 absent=dirty=release_bad=mutate_get=replace_on_submit=bad_get=rc_submit=rc_cancel=rc_step=rc_feed=rc_ready=0;
 generation=7;feed_generation=9;n_installed=n_staged=n_feed=1;
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};current=&rt;
 volume=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(volume)};
 feed=(installed_apps_feed_v1){.base={.base={.api_version=1,.struct_size=sizeof(feed),.refresh=refresh,.generation=gen,.count=count,.get=get,.step=step,.cancel=cancel,.remove=remove_app,.staged_count=staged,.staged_get=get,.install=install,.network_ready=netready},.provision_tag=INSTALLED_APPS_PROVISION_TAG,.provision_version=1,.install_descriptor=provision},.feed_tag=INSTALLED_APPS_FEED_TAG,.feed_version=1,.refresh=frefresh,.step=fstep,.cancel=fcancel,.generation=fgen,.count=fcount,.get=get,.install=finstall};
 update=(software_update_v1){1,sizeof(update),NULL,urefresh,ustep,status,uget,begin,yes,yes,yes};
 row=(installed_app_v1){.available=true};strcpy(row.id,"notes");strcpy(row.name,"Notes");strcpy(row.version,"1");strcpy(row.launch_path,"sd/apps/notes.elf");
 app_store_inventory_init(&state,&rt,(app_store_inventory_hooks){NULL,alive,keep});
}
static void open_catalog(void){assert(app_store_inventory_open(&state,&update)==0);if(state.available_feed)assert(!app_store_inventory_set_feed(&state,true));}
static app_store_inventory_selection choose(uint32_t i){app_store_inventory_selection s;assert(app_store_inventory_choose(&state,i,&s)==0);return s;}
int main(void){unsigned tests=0;
#define TEST(code) do{reset();code;tests++;}while(0)
 TEST(absent=1;assert(app_store_inventory_open(&state,&update)==2);assert(app_store_inventory_view(&state)->step(&state));assert(update_steps==1&&!volume_acquires);assert(!app_store_inventory_close(&state)));
 TEST(absent=dirty=1;assert(app_store_inventory_open(&state,&update)==-9);assert(retained==1&&!releases));
 TEST(open_catalog();software_update_status_v1 s={.struct_size=sizeof(s)};assert(state.facade.status(&state,&s)&&s.count==4);assert(!volume_acquires));
 TEST(open_catalog();for(unsigned i=0;i<4;i++){app_store_inventory_selection s=choose(i);assert(s.kind==i);}assert(!volume_acquires));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);generation++;assert(app_store_inventory_confirm(&state,&s)==3&&!submits&&!volume_acquires));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);strcpy(row.version,"2");assert(app_store_inventory_confirm(&state,&s)==3&&!submits));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);strcpy(row.launch_path,"other.elf");assert(app_store_inventory_confirm(&state,&s)==3&&!submits));
 TEST(open_catalog();mutate_get=1;app_store_inventory_selection s;assert(app_store_inventory_choose(&state,1,&s)==3));
 TEST(open_catalog();bad_get=1;app_store_inventory_selection s;assert(app_store_inventory_choose(&state,1,&s)==-9&&retained));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);assert(!app_store_inventory_confirm(&state,&s)&&submits==1&&volume_acquires==1);rc_step=1;assert(state.facade.step(&state)&&!releases);rc_step=0;assert(state.facade.step(&state)&&releases==1&&submits==1);assert(!app_store_inventory_close(&state)&&releases==2));
 TEST(open_catalog();app_store_inventory_selection s=choose(2);assert(!app_store_inventory_confirm(&state,&s)&&state.installing);assert(state.facade.step(&state)&&!strcmp(state.result,"App installed; ready to open")));
 TEST(open_catalog();app_store_inventory_selection s=choose(3);feed_generation++;assert(app_store_inventory_confirm(&state,&s)==3&&!submits));
 TEST(open_catalog();row.available=false;app_store_inventory_selection s=choose(3);assert(app_store_inventory_confirm(&state,&s)==2&&!submits));
 TEST(open_catalog();app_store_inventory_selection s=choose(3);assert(!app_store_inventory_confirm(&state,&s)&&submits==1));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);assert(!app_store_inventory_confirm(&state,&s));rc_step=2;assert(state.facade.step(&state)&&app_store_inventory_network_needed(&state));assert(!app_store_inventory_network_ready(&state,1234)&&ready_calls==1&&!app_store_inventory_network_needed(&state)));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);assert(!app_store_inventory_confirm(&state,&s));rc_cancel=1;assert(app_store_inventory_close(&state)==1&&!releases);assert(state.facade.step(&state)&&polls==0);rc_cancel=0;assert(!app_store_inventory_close(&state)&&releases==2&&submits==1));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);rc_submit=-3;assert(app_store_inventory_confirm(&state,&s)==-9&&!releases);int calls=cancels;assert(app_store_inventory_close(&state)==-9&&calls==cancels));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);rc_submit=77;assert(app_store_inventory_confirm(&state,&s)==-9&&!releases));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);replace_on_submit=1;assert(app_store_inventory_confirm(&state,&s)==-9&&!releases));
 TEST(open_catalog();release_bad=1;assert(app_store_inventory_close(&state)==-9&&retained));
 TEST(open_catalog();assert(state.facade.refresh(&state,1234));rc_feed=1;assert(state.facade.step(&state)&&feed_steps==1);rc_cancel=1;assert(app_store_inventory_close(&state)==1&&!releases);rc_cancel=0;assert(!app_store_inventory_close(&state)&&feed_cancels==2));
 TEST(open_catalog();assert(state.facade.refresh(&state,1234));rc_feed=99;assert(!state.facade.step(&state)&&retained&&!feed_cancels));
 TEST(open_catalog();assert(!state.facade.begin(&state,1,1234)&&!begins);assert(state.facade.begin(&state,0,1234)&&begins==1));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);rc_submit=-2;assert(app_store_inventory_confirm(&state,&s)==3&&releases==1&&!state.operation));
 TEST(open_catalog();n_installed=41;software_update_status_v1 s={.struct_size=sizeof(s)};assert(!state.facade.status(&state,&s)&&retained));
 TEST(open_catalog();current=&other;int n=acquires;assert(app_store_inventory_close(&state)==-9&&acquires==n&&!releases));
 TEST(feed.base.base.struct_size=INSTALLED_APPS_LOCAL_PREFIX_SIZE;open_catalog();assert(!state.feed);app_store_inventory_selection s=choose(2);assert(s.kind==APP_STORE_STAGED));
 TEST(open_catalog();assert(!app_store_inventory_close(&state));assert(!app_store_inventory_open(&state,&update))); 
 TEST(assert(!app_store_inventory_open(&state,&update));assert(!state.feed&&state.available_feed);software_update_status_v1 s={.struct_size=sizeof(s)};assert(state.facade.status(&state,&s)&&s.count==3);assert(!app_store_inventory_set_feed(&state,true));assert(state.facade.status(&state,&s)&&s.count==4));
 TEST(open_catalog();assert(state.facade.refresh(&state,1234));assert(app_store_inventory_set_feed(&state,false)==1);assert(!app_store_inventory_cancel(&state));assert(!app_store_inventory_set_feed(&state,false)));
 TEST(rt.release=NULL;assert(app_store_inventory_open(&state,&update)==-9&&!acquires));
 TEST(rt.struct_size=1;assert(app_store_inventory_open(&state,&update)==-9&&!acquires));
 TEST(feed.base.base.generation=NULL;open_catalog();software_update_row_v1 out={.struct_size=sizeof(out)};assert(state.facade.get(&state,1,&out)&&out.availability==SOFTWARE_UPDATE_CURRENT);app_store_inventory_selection s=choose(1);assert(app_store_inventory_confirm(&state,&s)==2&&!submits&&!volume_acquires));
 TEST(open_catalog();app_store_inventory_selection selected=choose(1);assert(!app_store_inventory_confirm(&state,&selected));rc_cancel=1;assert(app_store_inventory_cancel(&state)==1);software_update_status_v1 s={.struct_size=sizeof(s)};assert(state.facade.status(&state,&s)&&s.resources_open&&s.state==SOFTWARE_UPDATE_PREPARING));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);char route[193];assert(!app_store_inventory_launch_route(&state,&s,route,sizeof(route))&&!strcmp(route,row.launch_path)&&!volume_acquires&&!submits));
 TEST(open_catalog();app_store_inventory_selection s=choose(2);char route[193];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==3&&!route[0]));
 TEST(open_catalog();app_store_inventory_selection s=choose(3);char route[193];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==3));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);generation++;char route[193];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==3));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);strcpy(row.launch_path,"other.elf");char route[193];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==3));
 TEST(open_catalog();row.available=false;app_store_inventory_selection s=choose(1);char route[193];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==3));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);char route[4];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==3&&!route[0]));
 TEST(open_catalog();strcpy(row.launch_path,"../unsafe.elf");app_store_inventory_selection s=choose(1);char route[193];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==3));
 TEST(open_catalog();app_store_inventory_selection s=choose(1);current=&other;char route[193];assert(app_store_inventory_launch_route(&state,&s,route,sizeof(route))==-9));
 printf("%u inventory cases PASS\n",tests);return 0;
}
