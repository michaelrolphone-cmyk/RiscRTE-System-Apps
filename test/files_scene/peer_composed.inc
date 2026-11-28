/* Native navigation only; production peer/network/WebDAV/storage controllers.
 * Synthetic BLE/Wi-Fi/TCP and backing KV model external endpoints. */
static unsigned peer_stage;static bool peer_reject,peer_direct;
static const char peer_source_bytes[]="v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
static unsigned peer_written,peer_finished,peer_opened;
static uint32_t peer_source_count(const char*n){assert(!strcmp(n,"incoming.obj"));return 1;}
static bool peer_source_get(const char*n,uint32_t i,t5_file_handler_t*out){assert(peer_source_count(n)&&!i);memset(out,0,sizeof(*out));out->kind=T5_FILE_HANDLER_APP;strcpy(out->app_id,"model_viewer");strcpy(out->display_name,"Model Viewer");return true;}
static bool peer_source_supported(const char*id){assert(!strcmp(id,"model_viewer"));return true;}
static int32_t peer_source_begin(const char*n,uint64_t size,uint64_t*token){assert(peer_source_count(n)&&size==sizeof(peer_source_bytes)-1&&!peer_written);*token=21;return 0;}
static int32_t peer_source_write(uint64_t token,const void*b,uint32_t n,uint32_t*out){assert(token==21&&n&&peer_written+n<=sizeof(peer_source_bytes)-1);assert(!memcmp(b,peer_source_bytes+peer_written,n));*out=n>5?5:n;peer_written+=*out;return 0;}
static int32_t peer_source_finish(uint64_t token){assert(token==21&&peer_written==sizeof(peer_source_bytes)-1);assert(++peer_finished==1);return 0;}
static int32_t peer_source_discard(uint64_t token){(void)token;assert(0);return 0;}
static bool peer_source_request(uint64_t token,const char*id,uint64_t cookie){assert(token==21&&peer_source_supported(id)&&cookie==UINT64_C(0x4642524f57534552)&&peer_finished==1);assert(!fpeer&&!fbm_mounted&&fsh_phase==FSH_OFF&&!files_ui.token&&!fsource_input&&!held&&!presenting);assert(++peer_opened==1);stage=3;return true;}
static bool peer_source_result(int32_t*error,uint64_t*cookie){(void)error;(void)cookie;return false;}
static const risc_file_open_source_api_v1 peer_source_api={.api_version=1,.struct_size=sizeof(peer_source_api),.handler_count=peer_source_count,.handler_get=peer_source_get,.handler_supported=peer_source_supported,.begin=peer_source_begin,.write=peer_source_write,.finish=peer_source_finish,.discard=peer_source_discard,.open_request=peer_source_request,.take_result=peer_source_result};
static void peer_action(unsigned action){
 unsigned target=0;for(unsigned i=0;i<files_ui.committed.node_count;i++)if(files_ui.committed.nodes[i].action==action){target=files_ui.committed.nodes[i].id;break;}
 if(!target)return;
 risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=0;assert(api->snapshot(api->context,files_ui.token,&n,&flags)==0);
 if(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))return;
 nav_pressed=n.focus[n.depth-1]==target?RISC_NAV_CONFIRM:RISC_NAV_DOWN;
}
static void root_yield(uint32_t n){
 ms+=n;if(++yields==30000)fprintf(stderr,"peer timeout stage=%u mode=%u phase=%u peer=%u count=%u dirty=%u mounted=%u status=%s share=%s\n",peer_stage,fb_mode,fsh_phase,fpeer_status.state,files_scene_peer_count,fb_dirty,fbm_mounted,fb_status,fsh_message);assert(yields<30000);peer_backend_tick(fsh_now);
 if(!files_ui.token||held||presenting||!submits||fb_dirty)return;
 if(peer_stage==0){if(fb_mode==FB_LIST){peer_action(FP_OPTIONS);return;}assert(fb_mode==FB_OPTIONS);peer_action(FP_OPTION+7);if(nav_pressed==RISC_NAV_CONFIRM)peer_stage=1;return;}
 if(peer_stage==1){if(fpeer_status.state==PEER_DISCOVERING){peer_action(FP_PEER);return;}if(fpeer_status.state!=PEER_PAIRING)return;
  bool number=false;for(unsigned i=0;i<files_ui.committed.node_count;i++)if(!strcmp(files_ui.committed.nodes[i].text,"123456"))number=true;
  assert(number);peer_action(peer_reject?FP_PEER_REJECT:FP_PEER_MATCH);if(nav_pressed==RISC_NAV_CONFIRM)peer_stage=2;return;
 }
 if(peer_stage==2){
  if(peer_reject){if(fsh_phase!=FSH_OFF)return;peer_stage=3;}
  else {if(!fbm_mounted||fb_mode!=FB_LIST)return;assert(fb_storage_index==FB_REMOTE_ROOT&&fb_count);bool found=false;for(unsigned i=0;i<fb_count;i++)if(!strcmp(fb_rows[i].name,"incoming.obj"))found=true;assert(found);if(peer_preview_case||peer_delete_case||peer_source_case){unsigned row=0;while(row<fb_count&&strcmp(fb_rows[row].name,"incoming.obj"))++row;peer_action(FP_ROW+row);if(nav_pressed==RISC_NAV_CONFIRM)peer_stage=4;return;}nav_pressed=RISC_NAV_BACK;peer_stage=3;return;}
 }
 if(peer_stage==4){if(fb_mode!=FB_ACTIONS)return;peer_action(FP_OPERATION+(peer_source_case?0:peer_preview_case?2:6));if(nav_pressed==RISC_NAV_CONFIRM)peer_stage=5;return;}
 if(peer_stage==5){
  if(peer_source_case)return;
  if(peer_preview_case){if(!fb_preview_count)return;assert(fb_preview_count==sizeof(peer_source_bytes)-1&&!memcmp(fb_preview,peer_source_bytes,fb_preview_count));peer_stage=6;}
  else {if(peer_backend_remote_exists("/incoming.obj")){peer_action(fp_modal?FP_CONFIRM:FP_REVIEW);return;}peer_stage=6;}
 }
 if(fsh_phase!=FSH_OFF){nav_pressed=RISC_NAV_BACK;return;}
 assert(!fpeer&&!fbm_mounted);nav_pressed=RISC_NAV_HOME;stage=3;
}
