/* One production adapter/controller executable, supplied different native
 * inventories at runtime. The metadata TSV is a test-owner transport only. */
#define main previous_watch_main
#define risc_runtime_get_api previous_watch_runtime
#include "watch_native_springboard_test.c"
#undef main
#undef risc_runtime_get_api
#include "RiscAppCatalogV1.h"
static risc_app_catalog_entry_v1 inventory[RISC_APP_CATALOG_MAX];
static uint32_t inventory_count;
static uint64_t inventory_generation=1;
static unsigned fault,catalog_calls,catalog_acquires,catalog_releases,runtime_calls,snapshot_calls;
static bool retained,mutate_guard,mutate_drain;
static risc_display_output_api_v1 owned_display;
static risc_app_catalog_v1 owned_catalog;
static int32_t inventory_snapshot(void *context,risc_app_catalog_snapshot_v1 *out) {
 (void)context;assert(!retained);catalog_calls++;snapshot_calls++;
 if(fault==5)return RISC_APP_CATALOG_UNAVAILABLE;
 if(fault==6)return RISC_APP_CATALOG_CONTEXT;
 if(fault==7)return RISC_APP_CATALOG_RETAINED;
 *out=(risc_app_catalog_snapshot_v1){.struct_size=sizeof(*out),.count=inventory_count,.generation=inventory_generation};
 if(fault==8)out->count=RISC_APP_CATALOG_MAX+1;
 if(fault==9)out->generation=0;
 if(fault==10)out->struct_size--;
 if(fault==11 && snapshot_calls%2==0)out->generation++;
 return RISC_APP_CATALOG_OK;
}
static int32_t inventory_get(void *context,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1 *out) {
 (void)context;assert(!retained);catalog_calls++;
 if(fault==12)return RISC_APP_CATALOG_CONTEXT;
 if(fault==13)return RISC_APP_CATALOG_RETAINED;
 if(fault==14)return RISC_APP_CATALOG_INVALID;
 if(generation!=inventory_generation)return RISC_APP_CATALOG_STALE;
 if(index>=inventory_count)return RISC_APP_CATALOG_NOT_FOUND;
 *out=inventory[index];
 if(fault==15)memset(out->display_name,'X',sizeof(out->display_name));
 if(fault==16)strcpy(out->file_name,"../escape.elf");
 if(fault==17)out->flags=128;
 if(fault==18)out->struct_size--;
 if(fault==19 && index)strcpy(out->file_name,inventory[0].file_name);
 if(fault==20)strcpy(out->file_name,"/absolute.elf");
 if(fault==21)strcpy(out->display_name,"Control\ncharacter");
 if(fault==22)memset(out->icon,'X',sizeof(out->icon));
 if(fault==23)strcpy(out->file_name,"Apps//empty.elf");
 if(fault==24)strcpy(out->file_name,"Apps\\escape.elf");
 return RISC_APP_CATALOG_OK;
}
bool portable_app_before_launch(const char *destination) {
 assert(destination && !retained);
 if(mutate_guard){mutate_guard=false;inventory_generation++;}
 return true;
}
static bool inventory_present_status(void *context,risc_display_present_token_v1 token,risc_display_present_status_v1 *out) {
 assert(!retained);
 if(mutate_drain){mutate_drain=false;inventory_generation++;}
 return nt_status(context,token,out);
}
static bool inventory_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *out) {
 assert(!retained);runtime_calls++;
 if(!strcmp(name,"display.output")){out->api=&owned_display;grants++;return true;}
 if(strcmp(name,RISC_APP_CATALOG_CAPABILITY))return nt_acquire(name,version,id,out);
 catalog_acquires++;assert(version==1 && !id);
 if(fault==1 || fault==3)return false;
 if(fault==2){out->slot=31;out->generation=9;grants++;return false;}
 out->api=&owned_catalog;out->slot=31;out->generation=9;grants++;return true;
}
static bool inventory_release(risc_runtime_capability_v1 *grant) {
 assert(!retained);runtime_calls++;
 if(grant->api==&owned_catalog) {
  catalog_releases++;
  if(fault==25 || fault==26){if(fault==26)memset(grant,0,sizeof(*grant));return false;}
 }
 return nt_release(grant);
}
static bool inventory_health(risc_runtime_health_v1 *out) {assert(!retained);runtime_calls++;return fault!=3 && nt_health(out);}
static bool inventory_launch(const char *name) {assert(!retained);runtime_calls++;return nt_launch(name);}
static bool inventory_retain(void) {assert(!retained);retained=true;return true;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
 static risc_runtime_api_v1 runtime;
 runtime=nt_runtime;runtime.acquire=inventory_acquire;runtime.release=inventory_release;
 runtime.health=inventory_health;runtime.request_launch=inventory_launch;runtime.retain_invocation=inventory_retain;
 return version==1?&runtime:NULL;
}
static void load_inventory(const char *path) {
 FILE *file=fopen(path,"r");assert(file);char line[1024];
 while(fgets(line,sizeof(line),file)) {
  assert(inventory_count<RISC_APP_CATALOG_MAX);char *fields[4];fields[0]=line;
  for(unsigned i=1;i<4;i++){char *tab=strchr(fields[i-1],'\t');assert(tab);*tab=0;fields[i]=tab+1;}
  char *end=strchr(fields[3],'\n');assert(end);*end=0;
  risc_app_catalog_entry_v1 *entry=&inventory[inventory_count++];
  *entry=(risc_app_catalog_entry_v1){.struct_size=sizeof(*entry),.flags=atoi(fields[3])?RISC_APP_CATALOG_AVAILABLE:0};
  char *native_id=strchr(fields[3],'\t');
  if(native_id){*native_id++=0;assert(strlen(native_id)<sizeof(entry->id));strcpy(entry->id,native_id);}
  else snprintf(entry->id,sizeof(entry->id),"installed-%u",inventory_count);
  assert(strlen(fields[0])<sizeof(entry->display_name));strcpy(entry->display_name,fields[0]);
  assert(strlen(fields[1])<sizeof(entry->file_name));strcpy(entry->file_name,fields[1]);
  assert(strlen(fields[2])<sizeof(entry->icon));strcpy(entry->icon,fields[2]);
  if(!entry->flags)strcpy(entry->min_firmware_version,"99.0.0");
 }
 assert(!ferror(file));assert(!fclose(file));
}
static void no_more_callbacks(const t5_app_api_v1 *client) {
 unsigned before=runtime_calls+catalog_calls;
 t5_app_manifest_t entry;t5_app_input_t input;
 assert(!client->installed_apps_refresh());assert(!client->installed_apps_count());
 assert(!client->installed_apps_get(0,&entry));assert(!client->request_app_launch(0));
 assert(!client->poll(&input,0));app_main();app_module_fini();assert(app_module_init()==-1);
 assert(before==runtime_calls+catalog_calls);
}
static char messages[2048];
static t5_app_api_v1 observed_api,original_api;
static springboard_presentation observed_presentation;
static const springboard_presentation *original_presentation;
static void record_message(const char *message) {
 size_t used=strlen(messages),size=strlen(message);assert(used+size+2<sizeof(messages));
 memcpy(messages+used,message,size);messages[used+size]='\n';messages[used+size+1]=0;
}
static void observe_text(int32_t x,int32_t y,const char *text) {record_message(text);original_api.draw_text(x,y,text);}
static void observe_label(int32_t x,int32_t y,int32_t width,const char *text) {record_message(text);original_api.draw_label(x,y,width,text);}
static void observe_caption(int y,const char *text,bool clock,uint32_t color) {record_message(text);original_presentation->caption(y,text,clock,color);}
static void check_status_text(const t5_app_api_v1 *client,bool available) {
 api=client;assert(springboard_watch_open());original_api=*api;observed_api=original_api;
 observed_api.draw_text=observe_text;observed_api.draw_label=observe_label;api=&observed_api;
 catalog_available=available;count=client->installed_apps_count();selected=0;layout();
 if(!count) {
  messages[0]=0;raster_page(NULL);api->present(false);
  assert(strstr(messages,available?"No installed apps found.":"Installed apps unavailable."));
  assert(!strstr(messages,"SD"));
  original_presentation=springboard_presentation_get();assert(original_presentation);
  observed_presentation=*original_presentation;observed_presentation.caption=observe_caption;
  nova_state state={.view=&observed_presentation,.pending=-1};nova_layout(&state);
  messages[0]=0;nova_render(&state);assert(strstr(messages,available?"NO APPS":"APPS UNAVAILABLE"));
 } else {
  original_presentation=springboard_presentation_get();assert(original_presentation);
  nova_state state={.view=original_presentation,.pending=-1};nova_layout(&state);
  assert(state.n==(count>NOVA_PAGE_APPS?NOVA_PAGE_APPS:count));
  nova_render(&state);
  unsigned remaining=20;
  while(springboard_transition_active() && remaining--){ticks+=40;nova_render(&state);}
  assert(!springboard_transition_active());
  for(uint32_t i=0;i<count;i++)if(!inventory[i].flags) {
   selected=i;messages[0]=0;assert(!launch());assert(strstr(messages,"Application unavailable"));
   assert(!strstr(messages,"Requires firmware"));break;
  }
 }
 api=client;
}
static char persisted_pin[130];
static bool pin_exists(const char *path) {assert(!strcmp(path,HOME_APPS_PATH));return true;}
static bool pin_read(const char *path,void *buffer,size_t capacity,size_t *size) {
 assert(!strcmp(path,HOME_APPS_PATH));*size=strlen(persisted_pin);
 if(!buffer)return true;
 assert(capacity>=*size);memcpy(buffer,persisted_pin,*size);return true;
}
static bool pin_write(const char *path,const void *bytes,size_t size) {
 assert(!strcmp(path,HOME_APPS_PATH));assert(size<sizeof(persisted_pin));memcpy(persisted_pin,bytes,size);persisted_pin[size]=0;return true;
}
static const t5_storage_api_v1 pin_storage={.api_version=1,.struct_size=sizeof(pin_storage),.exists=pin_exists,.read_file=pin_read,.write_file_atomic=pin_write};
static void check_pin_rebinding(const t5_app_api_v1 *client) {
 if(inventory_count<2)return;
 t5_app_manifest_t first;assert(client->installed_apps_get(0,&first));strcpy(persisted_pin,first.file_name);strcat(persisted_pin,"\n");
 storage=&pin_storage;count=inventory_count;load_home_pins();assert(home_pinned[0]&&!home_pinned[1]);
 risc_app_catalog_entry_v1 a=inventory[0],b=inventory[1];unsigned original_count=inventory_count,before=launches;
 inventory[0]=b;inventory[1]=a;inventory_generation++;
 assert(!client->request_app_launch(0));assert(refresh_changed_catalog());assert(!home_pinned[0]&&home_pinned[1]);
 assert(save_home_pins());assert(!strcmp(persisted_pin,first.file_name)||!strncmp(persisted_pin,first.file_name,strlen(first.file_name)));
 inventory_count=1;inventory_generation++;assert(!client->request_app_launch(1));assert(refresh_changed_catalog());
 assert(!home_pinned[0]&&!home_pinned[1]&&launches==before);
 inventory[0]=a;inventory[1]=b;inventory_count=original_count;inventory_generation++;assert(client->installed_apps_refresh());
 storage=NULL;memset(home_pinned,0,sizeof(home_pinned));count=inventory_count;
}
static void check_stale_controller(const t5_app_api_v1 *client) {
 if(!inventory_count)return;
 api=client;assert(springboard_watch_open());count=client->installed_apps_count();selected=0;layout();
 check_pin_rebinding(client);
 unsigned before=launches,original_count=inventory_count;
 inventory_generation++;assert(!launch());assert(launches==before&&count==inventory_count&&selected==0&&catalog_available);
 inventory_count--;inventory_generation++;assert(!launch());
 assert(launches==before&&count==inventory_count&&selected==0&&catalog_available);
 inventory_count=original_count;inventory_generation++;assert(client->installed_apps_refresh());api=client;
}
static void exercise_controller(unsigned width,unsigned height) {
 fw=width;fh=height;pitch=fw+3;guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 use_async=true;contact_count=1;assert(app_module_init()==0);full_run=1;mutate_guard=true;
 app_main();assert(!mutate_guard&&!launches&&count==inventory_count&&catalog_available);
 full_run=0;app_module_fini();assert(!retained&&!grants&&!subscriptions&&!frames);guards();free(guarded);
}
static void exercise(unsigned width,unsigned height) {
 fw=width;fh=height;pitch=fw+3;guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 use_async=true;assert(app_module_init()==0);assert(!catalog_acquires);
 const t5_app_api_v1 *client=t5_app_get_api(1);assert(client);
 if(fault==4)owned_catalog.struct_size--;
 bool refreshed=client->installed_apps_refresh();
 if(fault && fault<25) {
  assert(!refreshed);assert(client->installed_apps_count()==0);
  if(fault==2 || fault==3 || fault==6 || fault==7 || fault==12 || fault==13) {
   assert(retained);no_more_callbacks(client);return;
  }
  assert(!retained);check_status_text(client,false);app_module_fini();assert(!grants&&!subscriptions&&!frames);free(guarded);return;
 }
 assert(refreshed && client->installed_apps_count()==inventory_count);
 check_status_text(client,true);
 assert(!client->installed_apps_get(0,NULL));t5_app_manifest_t item;
 assert(!client->installed_apps_get(UINT32_MAX,&item));assert(!client->request_app_launch(UINT32_MAX));
 for(uint32_t i=0;i<inventory_count;i++) {
  assert(client->installed_apps_get(i,&item));char expected_key[128];if(strlen(inventory[i].file_name)<sizeof(expected_key))strcpy(expected_key,inventory[i].file_name);
  else {strcpy(expected_key,"id:");strcat(expected_key,inventory[i].id);}
  assert(!strcmp(item.file_name,expected_key));
  assert(!strcmp(item.display_name,inventory[i].display_name));assert(!strcmp(item.icon,inventory[i].icon));
  unsigned before=launches;bool ok=client->request_app_launch(i);
  assert(ok==!!(inventory[i].flags&RISC_APP_CATALOG_AVAILABLE));assert(launches==before+(ok?1u:0u));
 }
 if(fault>=25){app_module_fini();assert(retained);no_more_callbacks(client);return;}
 check_stale_controller(client);
 if(inventory_count) {
  unsigned before=launches;mutate_guard=true;
  assert(!client->request_app_launch(0) && !mutate_guard && launches==before);
  assert(!client->installed_apps_count());assert(client->installed_apps_refresh());
  client->clear();client->present(false);mutate_drain=true;
  assert(!client->request_app_launch(0) && !mutate_drain && launches==before);
  assert(!client->installed_apps_count());assert(client->installed_apps_refresh());
  inventory_generation++;assert(!client->request_app_launch(0));assert(launches==before);
  assert(!client->installed_apps_count());assert(client->installed_apps_refresh());
  /* Added, removed and reordered installed identities change only native data. */
  inventory_count--;inventory_generation++;assert(client->installed_apps_refresh());assert(client->installed_apps_count()==inventory_count);
  if(inventory_count<RISC_APP_CATALOG_MAX) {
   inventory[inventory_count]=(risc_app_catalog_entry_v1){.struct_size=sizeof(inventory[0]),.flags=RISC_APP_CATALOG_AVAILABLE,
    .display_name="New installed app",.file_name="Apps/new/new.elf",.icon="solid:f017",.id="new-app"};
   inventory_count++;inventory_generation++;assert(client->installed_apps_refresh());assert(client->installed_apps_count()==inventory_count);
   assert(client->request_app_launch(inventory_count-1));assert(!strcmp(launched,"Apps/new/new.elf"));
  }
  inventory[0].flags=0;inventory_generation++;before=launches;assert(!client->request_app_launch(0));assert(launches==before);
  assert(client->installed_apps_refresh());assert(client->installed_apps_get(0,&item)&&!item.compatible);
 }
 app_module_fini();assert(!retained&&!grants&&!subscriptions&&!frames);guards();free(guarded);
}
int main(int argc,char **argv) {
 assert(argc>=5);load_inventory(argv[2]);
 owned_display=nt_display;owned_display.present_status=inventory_present_status;
 owned_catalog=(risc_app_catalog_v1){1,sizeof(owned_catalog),NULL,inventory_snapshot,inventory_get};
 if(!strcmp(argv[1],"parity"))return previous_watch_main(argc-2,argv+2);
 if(!strcmp(argv[1],"controller")){exercise_controller((unsigned)atoi(argv[3]),(unsigned)atoi(argv[4]));puts("Actual Nova stale publication recovery PASS");return 0;}
 assert(!strcmp(argv[1],"inventory") && (argc==5 || argc==6));fault=argc==6?(unsigned)atoi(argv[5]):0;
 exercise((unsigned)atoi(argv[3]),(unsigned)atoi(argv[4]));
 printf("Runtime inventory %u entries at %sx%s fault %u: metadata, launch authority and custody PASS\n",inventory_count,argv[3],argv[4],fault);
 return 0;
}
