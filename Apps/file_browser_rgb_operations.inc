/* Compact color presentation of the shared, capability-checked operations. */
static void fbr_draw(void) {
 portable_nova_begin();portable_nova_header(fb_mode==FB_DESTINATION?(fb_moving?"MOVE TO":"COPY TO"):fb_mode==FB_DELETE?"DELETE":fb_mode==FB_NOTICE?"FILE STATUS":"FILE ACTIONS");
 portable_nova_text(3,18,47,204,fb_mode==FB_DESTINATION?fb_path:fb_selected.name,NOVA_CYAN);
 if(fb_mode==FB_ACTIONS||fb_mode==FB_HANDLERS||fb_mode==FB_DESTINATION) {
  unsigned start=fb_mode==FB_ACTIONS?fb_action_page*2:0;
  unsigned count=fb_mode==FB_ACTIONS?FBX_ACTION_COUNT:fb_mode==FB_HANDLERS?fb_handler_count-fb_handler_offset:fb_count;
  for(unsigned row=0;row<2&&start+row<count;row++) {
   unsigned at=start+row;const char *name=fb_mode==FB_ACTIONS?fbx_labels[at]:fb_mode==FB_HANDLERS?fb_handlers[at].display_name:fb_rows[at].name;
   portable_nova_row(16,62+(int)row*58,208,54,name,"",at==fb_choice);
  }
  if(fb_feedback)portable_nova_text(3,18,175,204,fb_status,NOVA_CAP);
  if(fb_mode==FB_DESTINATION) {
   if(!fb_moving)portable_nova_button(184,5,44,38,"VOL",false);
   const char *labels[]={"Cancel","Up","Here","Next"};
   for(unsigned i=0;i<4;i++)portable_nova_button(16+(int)i*54,188,48,44,labels[i],i==2);
  } else {
   portable_nova_button(16,188,64,44,"Prev",false);
   portable_nova_button(88,188,64,44,"Open",false);
   portable_nova_button(160,188,64,44,"Next",false);
  }
 } else {
  portable_nova_wrap(1,18,68,204,18,6,fb_mode==FB_DELETE?"Delete this item permanently? This cannot be undone.":fb_status,NOVA_WHITE);
  if(fb_mode==FB_DELETE){portable_nova_button(16,188,100,44,"Cancel",fb_choice==0);portable_nova_button(124,188,100,44,"Delete",fb_choice==1);}
  else portable_nova_button(16,188,208,44,"Back",false);
 }
 fb_app->present(false);fb_dirty=false;
}
static bool fbr_touch(int x,int y) {
 if(fb_mode==FB_NOTICE){if(x>=16&&x<224&&y>=188&&y<232)return fb_back();return false;}
 if(fb_mode==FB_DELETE){if(y>=188&&y<232&&((x>=16&&x<116)||(x>=124&&x<224))){fb_choice=x<116?0:1;fbx_activate();fb_dirty=true;}return false;}
 if(fb_mode==FB_DESTINATION) {
  if(!fb_moving&&x>=184&&y<44){if(fbx_swap()){strcpy(fb_path,"/");(void)fb_load(0);}return false;}
  if(y>=188&&y<232&&x>=16){unsigned at=(unsigned)(x-16)/54;if(at<4&&(x-16)%54<48){
   if(at==0)fbx_cancel();else if(at==1){if(fb_parent(fb_path))(void)fb_load(0);}else if(at==2)fbx_commit_destination();else fb_page_next();}return false;}
 }
 if(x>=16&&x<224)for(unsigned row=0;row<2;row++)if(y>=62+(int)row*58&&y<116+(int)row*58){
  unsigned at=row+(fb_mode==FB_ACTIONS?fb_action_page*2:0),count=fb_mode==FB_ACTIONS?FBX_ACTION_COUNT:fb_mode==FB_HANDLERS?fb_handler_count-fb_handler_offset:fb_count;
  if(at<count){fb_choice=at;fbx_activate();fb_dirty=true;}return false;
 }
 if(y>=188&&y<232){if(x>=16&&x<80)fb_move(-1);else if(x>=88&&x<152)fb_activate();else if(x>=160&&x<224)fb_move(1);}
 return false;
}
