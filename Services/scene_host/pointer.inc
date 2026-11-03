/* Generic provider-owned continuous pointer surface. The inherited held-input
 * endpoint continues owning confirmation keys and lifecycle barriers. */
static pointer_gesture pointer_motion;
static bool pointer_owned[256];
static uint64_t pointer_last,pointer_sequence;
static risc_scene_pointer_event_v1 pointer_packet;
static void pointer_reset(bool deliver){
 bool had=pointer_active||pointer_motion.down||pointer_motion.dragging||pointer_pending;
 pointer_gesture_reset(&pointer_motion,true);
 pointer_active=false;memset(pointer_owned,0,sizeof(pointer_owned));
 if(deliver&&had){
  pointer_packet=(risc_scene_pointer_event_v1){.struct_size=sizeof(pointer_packet),.kind=RISC_POINTER_RESET};
  pointer_packet.intent.struct_size=sizeof(pointer_packet.intent);
  pointer_packet.intent.event=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.document_revision=document.revision};
  pointer_pending=true;
 }
}
static bool pointer_boundary(void){
 if(!pointer_enabled)return false;
 pointer_reset(true);return pointer_pending;
}
static const hit *pointer_hit(void){
 if(!pointer_enabled||!model->components)return NULL;
 for(unsigned i=0;i<visible.count;i++)if(visible.items[i].kind==HIT_POINTER)return &visible.items[i];
 return NULL;
}
static bool pointer_contains(const hit *h,int x,int y){return h&&x>=h->x&&x<h->x+h->w&&y>=h->y&&y<h->y+h->h;}
static bool pointer_sample(void){
 const hit *h=pointer_hit();
 if(!h){pointer_reset(true);return pointer_pending;}
 uint64_t now=clock_api->monotonic_ms(clock_api->context);
 if(!alive())return true;
 bool gap=pointer_last&&(now<pointer_last||now-pointer_last>100);
 pointer_last=now;
 if(gap){pointer_reset(true);return pointer_pending;}
 bool eligible=pointer_visible_revision==document.revision&&view_current()&&!handoff_waiting;
 bool consume=pointer_active,saw_up=false,rejected=false;
 for(unsigned i=0;i<raw_count;i++){
  const risc_touch_event_v1 *e=&raw_queue[(raw_head+i)%RAW_QUEUE_SIZE];
  int x=0,y=0;
  if(e->kind>=RISC_TOUCH_EVENT_DOWN&&e->kind<=RISC_TOUCH_EVENT_UP&&!touch_point(e->x,e->y,&x,&y))rejected=true;
  if(e->kind==RISC_TOUCH_EVENT_DOWN){
   if(pointer_active||pointer_contains(h,x,y)){
    consume=true;
    if(!eligible||saw_up||pointer_owned[e->id]||(!pointer_active&&!neutral))rejected=true;
    else {pointer_active=true;pointer_owned[e->id]=true;}
   }else if(pointer_active)rejected=true;
  }else if(pointer_active){
   if(!pointer_owned[e->id])rejected=true;
   if(e->kind==RISC_TOUCH_EVENT_UP){pointer_owned[e->id]=false;saw_up=true;}
   else if(e->kind!=RISC_TOUCH_EVENT_MOVE)rejected=true;
  }
 }
 if(!consume){
  if(!touch_state.contact_count&&!touch_state.buttons)pointer_gesture_reset(&pointer_motion,false);
  return false;
 }
 /* Do not let a consumed pad contact also activate shared chrome or scrolling. */
 raw_head=raw_count=0;last_sequence=capture_sequence;
 memcpy(dispatched_contacts,captured_contacts,sizeof(dispatched_contacts));dispatched_count=touch_state.contact_count;
 contact=false;top_pending=false;
 risc_touch_snapshot_v1 sample=touch_state;sample.width=(uint16_t)width;sample.height=(uint16_t)height;
 if(touch_state.buttons||sample.contact_count>2)rejected=true;
 for(unsigned i=0;i<sample.contact_count;i++){
  int x=0,y=0;if(!pointer_owned[sample.contacts[i].id]||!touch_point(sample.contacts[i].x,sample.contacts[i].y,&x,&y))rejected=true;
  sample.contacts[i].x=(uint16_t)x;sample.contacts[i].y=(uint16_t)y;
 }
 if(rejected||!eligible){pointer_reset(true);neutral=!sample.contact_count&&!sample.buttons;return true;}
 pointer_gesture_output out=pointer_gesture_sample_bounds(&pointer_motion,&sample,(uint32_t)now,h->x,h->y,h->x+h->w,h->y+h->h);
 if(!sample.contact_count){pointer_active=false;neutral=true;}
 if(out.dx||out.dy||out.wheel_x||out.wheel_y||out.click||out.button_changed){
  if(pointer_sequence==UINT64_MAX){pointer_reset(true);hold_fault=true;return true;}
  pointer_packet=(risc_scene_pointer_event_v1){.struct_size=sizeof(pointer_packet),.kind=RISC_POINTER_REPORT,
   .dx=out.dx,.dy=out.dy,.wheel_x=out.wheel_x,.wheel_y=out.wheel_y,.buttons=out.buttons,.click=out.click};
  pointer_packet.intent.struct_size=sizeof(pointer_packet.intent);
  int at=node_index(&document,h->node);
  pointer_packet.intent.event=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.document_revision=document.revision,.node=h->node,.action=at>=0?document.nodes[at].action:0,.sequence=++pointer_sequence};
  pointer_pending=true;
 }
 return true;
}
