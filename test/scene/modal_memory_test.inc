/* Extend the real modal fixture with all reachable page-buffer allocations.
 * These are host payload observations, never native allocator admission. */
void modal_memory_begin(unsigned);
size_t modal_memory_checkpoint_bytes(void);
void modal_memory_live(unsigned,size_t);
void modal_memory_end(unsigned,size_t,unsigned,unsigned);
static int modal_memory_page(uint64_t token){
 const risc_scene_page_api_v1 *pages=risc_scene_page_get_v1(api);assert(pages);
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};assert(!pages->geometry(NULL,&g));
 static uint8_t pixels[1024*1024/8];size_t length=(size_t)g.stride*g.height;
 assert(length<=sizeof(pixels));memset(pixels,0xff,length);
 risc_scene_page_document_v1 p={.struct_size=sizeof(p),.revision=2,
     .previous_action=11,.menu_action=12,.next_action=13,.back_action=14};
 return pages->present_page(NULL,token,&p,pixels,length);
}
static void modal_memory_run(unsigned continuity){
 const risc_scene_modal_api_v1 *modal=risc_scene_modal_get_v1(api);assert(modal);
 const risc_scene_components_api_v1 *components=risc_scene_components_get_v1(api);assert(components);
 const risc_scene_checkpoint_api_v1 *cp=risc_scene_checkpoint_get_v1(api);assert(cp);
 const risc_scene_lifecycle_api_v1 *life=risc_scene_lifecycle_get_v1(api);assert(life);
 assert(!api->close(NULL,session));
 size_t pixel=(size_t)w*h/8,checkpoint=modal_memory_checkpoint_bytes();
 /* Fresh component/text, retained page/text, and retained page/page. Repeat
  * successful cycles, then refuse every allocation ordinal in the max vector. */
 for(unsigned scenario=0;scenario<3;scenario++)for(unsigned trial=0;trial<8;trial++){
  unsigned fail=trial<3?0:trial-2,parent_page=scenario!=0,child_page=scenario==2;
  if(fail&&(scenario!=2))continue;
  modal_memory_begin(fail);
  risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,
      .root=1,.route_count=1,.screen_key=9,.routes={{.id=1,.back_action=9}}};
  uint64_t parent=0,child=0;assert(!components->open(NULL,&d,NULL,&parent));
  assert(!life->configure(NULL,parent,continuity?RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY:0));
  if(parent_page){
   int rc=modal_memory_page(parent);
   if(fail==1||fail==2){
    assert(rc==RISC_SCENE_UNAVAILABLE);modal_memory_live(0,0);assert(!api->close(NULL,parent));
    modal_memory_end(2,pixel,parent_page,child_page);continue;
   }
   assert(!rc);modal_memory_live(2,2*pixel);
   /* Returning to ordinary components does not release the page pair. */
   d.revision=3;assert(!components->update(NULL,parent,&d));modal_memory_live(2,2*pixel);
  }
  assert(!cp->pause(NULL,parent));d.revision=1;
  int rc=modal->open(NULL,&d,&child);
  if(fail==3){
   assert(rc==RISC_SCENE_UNAVAILABLE&&!child);modal_memory_live(2,2*pixel);
   assert(!cp->resume(NULL,parent,RISC_SCENE_RESUME_REDRAW));assert(!api->close(NULL,parent));
   modal_memory_end(3,pixel,parent_page,child_page);continue;
  }
  assert(!rc&&child&&child!=parent);
  modal_memory_live(1+2*parent_page,checkpoint+2*pixel*parent_page);
  if(child_page){
   rc=modal_memory_page(child);
   if(fail==4||fail==5){assert(rc==RISC_SCENE_UNAVAILABLE);modal_memory_live(3,checkpoint+2*pixel);}
   else {assert(!rc);modal_memory_live(5,checkpoint+4*pixel);}
  }
  assert(!modal->close(NULL,child));modal_memory_live(2*parent_page,2*pixel*parent_page);
  assert(!cp->resume(NULL,parent,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_FOCUS_CHANGED));
  assert(!api->close(NULL,parent));modal_memory_live(0,0);
  modal_memory_end(1+2*parent_page+2*child_page,pixel,parent_page,child_page);
 }
 assert(!held&&!presenting&&!subscribed&&driver->quiesce());driver->stop();
}
