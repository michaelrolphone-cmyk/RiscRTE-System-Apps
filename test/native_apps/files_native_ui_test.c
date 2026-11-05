/* Actual Files, adapter, raster and mounted-file providers. No network I/O. */
#define FILE_SHARED_INPUT_FIXTURE
#include "file_browser_mount_test.c"
static risc_display_output_api_v1 native_display;
static unsigned native_delay,native_due,native_cases;
static bool native_inject_previsible,native_injected;
static int native_x=-1,native_y=-1;
static void native_contacts(int*x,int*y){*x=native_x;*y=native_y;}
static bool native_info(void*c,risc_display_info_v1*i){bool ok=test_info(c,i);i->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool native_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){bool ok=test_submit(c,f,r,n,o,t);native_due=test_ticks+native_delay;return ok;}
static bool native_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){(void)c;(void)t;if(native_inject_previsible&&test_ticks<native_due){test_edge(RISC_TOUCH_EVENT_DOWN,1,FILE_TEST_WIDTH-25,25);test_edge(RISC_TOUCH_EVENT_UP,1,FILE_TEST_WIDTH-25,25);native_inject_previsible=false;native_injected=true;}s->state=test_ticks<native_due?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool native_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){bool ok=get_cap(n,v,i,g);if(ok&&!strcmp(n,"display.output"))g->api=&native_display;return ok;}
static void native_step(bool act){if(fb_dirty)fb_draw();test_ticks++;t5_app_input_t in={0};assert(fb_app->poll(&in,1));fbn_input(&in);if(act&&in.tapped)(void)fbn_touch(in.touch_x,in.touch_y);}
static void native_settle(void){native_x=native_y=-1;for(unsigned i=0;i<500;i++){native_step(false);if(!fb_dirty&&portable_files_native_ready(NULL,NULL)&&!paper_token)return;}assert(!"native visibility timeout");}
static void native_geometry(void){
 for(unsigned i=0;i<fbn_count;i++){fbn_target a=fbn_targets[i];assert(a.x>=0&&a.y>=0&&a.w>0&&a.h>0&&a.x+a.w<=FILE_TEST_WIDTH&&a.y+a.h<=FILE_TEST_HEIGHT);assert(fbn_hit(a.x+a.w/2,a.y+a.h/2)==(int)i);for(unsigned j=0;j<i;j++){fbn_target b=fbn_targets[j];assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y);}}
 for(unsigned y=0;y<FILE_TEST_HEIGHT;y++)for(unsigned x=FILE_TEST_WIDTH;x<FILE_TEST_STRIDE;x++)assert(fb_pixels[y*FILE_TEST_STRIDE+x]==0xa5a5);
 native_cases++;
}
static fbn_target native_find(unsigned action,unsigned value){for(unsigned i=0;i<fbn_count;i++)if(fbn_targets[i].action==action&&fbn_targets[i].value==value)return fbn_targets[i];assert(!"missing native target");return (fbn_target){0};}
static void native_tap(unsigned action,unsigned value){native_settle();fbn_target r=native_find(action,value);native_x=r.x+r.w/2;native_y=r.y+r.h/2;for(unsigned i=0;i<4;i++)native_step(true);native_x=native_y=-1;for(unsigned i=0;i<4;i++)native_step(true);native_settle();native_geometry();}
static void native_capture(const char*directory,const char*name){native_settle();native_geometry();char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",FILE_TEST_WIDTH,FILE_TEST_HEIGHT);for(unsigned y=0;y<FILE_TEST_HEIGHT;y++)for(unsigned x=0;x<FILE_TEST_WIDTH;x++){unsigned p=fb_pixels[y*FILE_TEST_STRIDE+x];unsigned char c[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};assert(fwrite(c,1,3,f)==3);}assert(!fclose(f));}
int main(int argc,char**argv){assert(argc==3);native_delay=(unsigned)atoi(argv[2]);peer_backend_begin();peer_backend_folders();borrowed=true;
 native_display=test_display;native_display.get_info=native_info;native_display.submit=native_submit;native_display.present_status=native_status;
 runtime=test_runtime;runtime.acquire=native_acquire;runtime.release=release_cap;runtime.retain_invocation=retain_owner;runtime.diagnostic=files_diagnostic;test_runtime_override=&runtime;test_touch_script=native_contacts;
 const portable_wifi_credentials saved={"fixture","synthetic"};assert(portable_wifi_profile_save(&kv,0,&saved)==0);memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0&&fb_open());assert(fb_page_rows==fbn_rows);
 if(native_delay){native_inject_previsible=true;fb_draw();for(unsigned i=0;i<200;i++){native_step(true);assert(fb_mode==FB_LIST);}native_settle();assert(native_injected&&!native_inject_previsible);native_cases++;}
 native_capture(argv[1],"root");
 native_tap(FBN_MENU,0);assert(fb_mode==FB_OPTIONS);native_tap(FBN_BACK,0);assert(fb_mode==FB_LIST);
 /* Held release after a layout change cannot invoke the replacement target. */
 native_settle();fbn_target menu=native_find(FBN_MENU,0);native_x=menu.x+5;native_y=menu.y+5;native_step(true);fb_mode=FB_OPTIONS;fb_choice=0;fb_dirty=true;native_step(true);native_x=native_y=-1;for(unsigned i=0;i<8;i++)native_step(true);assert(fb_mode==FB_OPTIONS);native_settle();native_tap(FBN_BACK,0);
 /* A provider row replacement between DOWN and UP cannot activate unseen data. */
 native_settle();fbn_target row=native_find(FBN_ROW,0);char saved_name[sizeof(fb_rows[0].name)];strcpy(saved_name,fb_rows[0].name);native_x=row.x+4;native_y=row.y+4;for(unsigned i=0;i<4;i++)native_step(true);strcpy(fb_rows[0].name,"unseen-replacement");native_x=native_y=-1;native_step(true);assert(fb_mode==FB_LIST);strcpy(fb_rows[0].name,saved_name);fb_dirty=true;native_settle();native_cases++;
 /* Unchanged hit maps retain a contact across repaint. */
 menu=native_find(FBN_MENU,0);native_x=menu.x+5;native_y=menu.y+5;for(unsigned i=0;i<4;i++)native_step(true);fb_dirty=true;native_step(true);native_x=native_y=-1;for(unsigned i=0;i<8;i++)native_step(true);assert(fb_mode==FB_OPTIONS);native_settle();native_tap(FBN_BACK,0);
 /* Native list swipe retains the original bounded page transition. */
 const risc_storage_volume_api_v1_ext*ext=risc_storage_volume_extension(fb_volume);assert(ext&&ext->mkdir(fb_volume->context,"/NativeCases"));for(unsigned i=0;i<12;i++){char path[64];snprintf(path,sizeof(path),"/NativeCases/item%02u.txt",i);risc_storage_file_t f=fb_volume->file_open_write(fb_volume->context,path);assert(f&&fb_volume->file_write(fb_volume->context,f,"x",1)==1&&fb_volume->file_close(fb_volume->context,f,true));}
 char large[577];for(unsigned i=0;i<sizeof(large);i++)large[i]=(char)('A'+i%26);risc_storage_file_t big=fb_volume->file_open_write(fb_volume->context,"/NativeCases/large.txt");assert(big&&fb_volume->file_write(fb_volume->context,big,large,sizeof(large))==sizeof(large)&&fb_volume->file_close(fb_volume->context,big,true));
 strcpy(fb_path,"/NativeCases");assert(fb_load(0));native_settle();row=native_find(FBN_ROW,0);native_x=row.x+10;native_y=row.y+row.h-4;for(unsigned i=0;i<4;i++)native_step(true);native_y-=40;for(unsigned i=0;i<4;i++)native_step(true);native_x=native_y=-1;for(unsigned i=0;i<8;i++)native_step(true);assert(fb_mode==FB_LIST&&fb_page==1);native_settle();native_cases++;
 /* Traverse actual storage reads forward and backward without losing a byte. */
 assert(fb_load(0));mount_action("large.txt",2);assert(fb_mode==FB_PREVIEW&&fb_size==577);native_settle();
 for(unsigned hex=0;hex<2;hex++){
  if(hex){native_tap(FBN_OPEN,0);assert(fb_hex);}
  uint64_t positions[128];unsigned pages=0;
  for(;;){assert(pages<128);unsigned divisor=fb_hex?3:1;assert(fbn_text_offset%divisor==0&&fbn_text_end%divisor==0);positions[pages++]=fb_offset+fbn_text_offset/divisor;uint64_t end=fb_offset+fbn_text_end/divisor;assert(end>positions[pages-1]&&end<=fb_size);if(end==fb_size)break;native_tap(FBN_NEXT,0);assert(fb_offset+fbn_text_offset/divisor==end);}
  for(unsigned i=pages-1;i;i--){native_tap(FBN_PREV,0);assert(fb_offset+fbn_text_offset/(fb_hex?3:1)==positions[i-1]);}
  assert(!fb_offset&&!fbn_text_offset);native_cases++;
 }
 assert(!fb_back());assert(!fb_back());
 strcpy(fb_path,"/Downloads");assert(fb_load(0));native_capture(argv[1],"local-list");fb_choice=mount_find("local.txt");fb_activate_file(fb_choice);assert(fb_mode==FB_ACTIONS);native_capture(argv[1],"actions");
 /* Every action is rendered/reachable on narrow screens, not only first two. */
 for(unsigned choice=0;choice<FBX_ACTION_COUNT;choice++){fb_choice=choice;fb_dirty=true;native_settle();native_geometry();(void)native_find(FBN_ROW,choice);}
 fb_choice=2;fbx_activate();assert(fb_mode==FB_PREVIEW);native_capture(argv[1],"preview");
 /* Formatter pages consume every byte of a full text and hex read exactly. */
 for(unsigned hex=0;hex<2;hex++){fb_hex=hex;fb_preview_count=hex?64:192;for(unsigned i=0;i<fb_preview_count;i++)fb_preview[i]=(unsigned char)(32+i%95);fbn_text_offset=0;fb_dirty=true;native_settle();unsigned consumed=0;for(unsigned page=0;page<100;page++){assert(fbn_text_offset==consumed&&fbn_text_end>consumed);consumed=fbn_text_end;if(consumed==fbn_text_length)break;native_tap(FBN_NEXT,0);}assert(consumed==192);native_cases++;}
 assert(!fb_back());assert(!fb_back());fb_mode=FB_LIST;mount_action("local.txt",6);native_capture(argv[1],"delete-confirmation");native_tap(FBN_CONFIRM,0);assert(fb_mode==FB_ACTIONS);assert(!fb_back());
 fb_list_choice=0;fb_option(7);peer_until(PEER_DISCOVERING);peer_step();native_settle();fpeer_choice=0;fpeer_action();peer_until(PEER_PAIRING);native_capture(argv[1],"pairing");
 fbn_target match=native_find(FBN_PEER,1);uint32_t old_pair=fpeer_status.pairing_generation;fpeer_status.pairing_generation++;assert(!fbn_touch(match.x+4,match.y+4)&&fb_dirty&&fpeer_pair_generation==old_pair);fpeer_status.pairing_generation=old_pair;native_settle();native_cases++;
 native_tap(FBN_PEER,1);peer_until(PEER_READY);native_capture(argv[1],"remote-list");
 mount_action("incoming.obj",2);mount_idle();native_capture(argv[1],"remote-preview");assert(!fb_back());assert(!fb_back());mount_action("incoming.obj",5);mount_idle();native_capture(argv[1],"destination");native_tap(FBN_BACK,0);mount_idle();
 (void)fsh_back();for(unsigned i=0;i<30000&&fsh_phase!=FSH_OFF;i++)peer_step();assert(portable_file_browser_close());assert(paper_frame_drain());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs&&!retains);peer_backend_end();printf("Files native %ux%u delay=%u actual adapter/raster/input: %u assertions groups PASS\n",FILE_TEST_WIDTH,FILE_TEST_HEIGHT,native_delay,native_cases);return 0;
}
