static int32_t share_observe(void*c){(void)c;return RISC_STORAGE_STATE_READY;}
static bool share_direct,share_reject,share_setup;static unsigned share_stage;
static bool share_error(void*c,char*out,size_t n){(void)c;assert(n);out[0]=0;return true;}
static bool share_ready(void*c){(void)c;return true;}
static bool share_stat(void*c,const char*p,uint64_t*n,bool*d){(void)c;*n=0;*d=!strcmp(p,"/");return *d;}
static risc_storage_dir_t share_dir(void*c,const char*p){(void)c;assert(!strcmp(p,"/"));return 1;}
static bool share_next(void*c,risc_storage_dir_t t,risc_storage_dirent_v1*out){(void)c;(void)out;assert(t==1);return false;}
static void share_dir_close(void*c,risc_storage_dir_t t){(void)c;assert(t==1);}
static void share_action(unsigned action){
 unsigned target=0;for(unsigned i=0;i<files_ui.committed.node_count;i++)if(files_ui.committed.nodes[i].action==action){target=files_ui.committed.nodes[i].id;break;}
 if(!target)return;
 risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=0;assert(!api->snapshot(api->context,files_ui.token,&n,&flags));if(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))return;
 nav_pressed=n.focus[n.depth-1]==target?RISC_NAV_CONFIRM:RISC_NAV_DOWN;
 if(nav_pressed==RISC_NAV_CONFIRM)fprintf(stderr,"share confirm action=%u stage=%u revision=%u time=%llu\n",action,share_stage,files_ui.committed.revision,(unsigned long long)ms);
}
static void root_yield(uint32_t n){
 ms+=n;if(cancel_calls)quiescent=true;
 if(setup_owned&&setup_state==RISC_SETUP_ADVERTISING)setup_state=RISC_SETUP_PAIR_CONFIRM;
 if(++yields==30000)fprintf(stderr,"share timeout stage=%u phase=%u direct=%u setup=%u/%u msg=%s\n",share_stage,fsh_phase,fsh_direct_stage,fsh_setup_status.state,fsh_setup_status.provider_state,fsh_message);
 assert(yields<30000);
 if(!files_ui.token||held||presenting||!submits)return;
 if(share_stage==0){if(fb_mode==FB_LIST){share_action(FP_OPTIONS);return;}assert(fb_mode==FB_OPTIONS);share_action(FP_OPTION+6);if(nav_pressed==RISC_NAV_CONFIRM)share_stage=1;return;}
 if(share_stage==1){if(fsh_direct!=share_direct){share_action(FP_TRANSPORT);return;}share_action(FP_SHARE);if(nav_pressed==RISC_NAV_CONFIRM)share_stage=2;return;}
 if(fsh_setup_screen&&fsh_setup_status.provider_state==RISC_SETUP_PAIR_CONFIRM){
  bool number=false;for(unsigned i=0;i<files_ui.committed.node_count;i++)if(!strcmp(files_ui.committed.nodes[i].text,"123456"))number=true;
  if(!number){return;}assert(!setup_confirms&&!begin_calls);share_action(share_reject?FP_SETUP_REJECT:FP_SETUP_MATCH);return;
 }
 if(share_stage==2){
  if(share_reject){if(fsh_phase!=FSH_OFF)return;assert(setup_confirms==1&&!begin_calls&&!listeners);share_stage=4;}
  else {if(fsh_status.state!=PORTABLE_FILE_SHARING_LISTENING)return;assert(listeners==1&&strlen(fsh_status.password)==16);if(share_direct)assert(setup_confirms==1&&begin_calls==1);if(share_setup){share_action(FP_SETUP);if(nav_pressed==RISC_NAV_CONFIRM)share_stage=3;return;}share_stage=4;}
 }
 if(share_stage==3){if(setup_confirms!=1||fsh_setup_status.provider_state!=RISC_SETUP_READY)return;nav_pressed=RISC_NAV_BACK;share_stage=4;return;}
 if(fsh_phase!=FSH_OFF||fsh_setup_screen){nav_pressed=RISC_NAV_BACK;return;}
 assert(!listeners&&!owned&&!setup_owned&&!direct_ble_on&&(!share_direct||share_reject||cancel_calls==1));nav_pressed=RISC_NAV_HOME;stage=3;
}
