/* Real static Springboard app_main with physical touch and test-only storage. */
#define main inherited_native_springboard_main
#define risc_runtime_get_api inherited_fixture_runtime_api
#include "watch_native_springboard_test.c"
#undef risc_runtime_get_api
#undef main
static risc_runtime_api_v1 fallback_runtime;
static risc_touch_api_v1 fallback_touch;
static unsigned fallback_case,pin_writes;
static bool storage_enabled;
static char saved_pins[512];
static unsigned right_x(void){return fw==410?300u:180u;}
static bool scripted_poll(void *context,size_t count_events) {
 unsigned next=polls+1;contact_down=false;
 if(next==3 || (fallback_case==2&&next==9)) {
  contact_down=true;contact_x=fallback_case==2?fw-40: fallback_case==3?fw/2:right_x();
  contact_y=fallback_case==2?32:fallback_case==3?fh-28:100;
 } else if((fallback_case==2||fallback_case==3)&&next==6) {
  contact_down=true;contact_x=right_x();contact_y=100;
 }
 return nt_poll_touch(context,count_events);
}
static bool scripted_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
 bool result=nt_acquire(name,version,instance,out);
 if(result&&!strcmp(name,"input.touch.raw"))out->api=&fallback_touch;
 return result;
}
static bool scripted_exit(void){return polls>=16;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&fallback_runtime:NULL;}
static bool pins_exist(const char *path){assert(!strcmp(path,HOME_APPS_PATH));return false;}
static bool pins_read(const char *path,void *bytes,size_t capacity,size_t *size){(void)path;(void)bytes;(void)capacity;(void)size;assert(!"absent test pin file must not be read");return false;}
static bool pins_write(const char *path,const void *bytes,size_t size) {
 assert(!strcmp(path,HOME_APPS_PATH)&&size<sizeof(saved_pins));memcpy(saved_pins,bytes,size);saved_pins[size]=0;pin_writes++;return true;
}
static const t5_storage_api_v1 fallback_storage={.api_version=1,.struct_size=sizeof(fallback_storage),.exists=pins_exist,.read_file=pins_read,.write_file_atomic=pins_write};
const t5_storage_api_v1 *t5_storage_get_api(uint32_t version){return storage_enabled&&version==1?&fallback_storage:NULL;}
int main(int argc,char **argv) {
 assert(argc==5);fw=(unsigned)atoi(argv[1]);fh=(unsigned)atoi(argv[2]);use_async=!strcmp(argv[3],"async");select_metadata(argv[4]);assert(!expect_presentation);
 pitch=fw+3;guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 fallback_touch=nt_touch;fallback_touch.poll=scripted_poll;
 fallback_runtime=nt_runtime;fallback_runtime.acquire=scripted_acquire;fallback_runtime.exit_requested=scripted_exit;
 for(fallback_case=1;fallback_case<=3;fallback_case++) {
  storage_enabled=fallback_case==2;pin_writes=0;saved_pins[0]=0;
  open_controller();assert(!production&&!nova_run());
  unsigned expected=fallback_case==3?(unsigned)page_size+1u:1u;
  assert(expected<count);app_main();
  if(fallback_case==2) {
   assert(!launches&&!edit_mode&&home_pinned[1]&&pin_writes==1);
   char expected_pins[256];snprintf(expected_pins,sizeof(expected_pins),"%s\n",portable_catalog[1].file_name);assert(!strcmp(saved_pins,expected_pins));
  } else {assert(launches==1&&selected==expected&&!strcmp(launched,portable_catalog[expected].file_name));}
  guards();close_controller();
 }
 free(guarded);printf("Native fallback %ux%u %s %s: physical right-column launch, edit/pin/done and page-dot launch PASS\n",fw,fh,argv[3],argv[4]);return 0;
}
