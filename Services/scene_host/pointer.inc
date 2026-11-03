/* Generic copied pointer intents. Physical input, frame identity and gestures
 * stay in the provider; ordered edges are validated before any domain intent. */
static pointer_gesture pointer_motion;
static risc_touch_snapshot_v1 pointer_cursor;
static bool pointer_rejecting,pointer_gap,pointer_clock_seen;
static uint64_t pointer_last;
static risc_scene_pointer_event_v1 pointer_packet;
static void pointer_sync(void){pointer_cursor=touch_state;}
static void pointer_reset(bool deliver){
 bool had=pointer_active||pointer_motion.down||pointer_motion.dragging||pointer_pending;
 pointer_rejecting=deliver&&(pointer_rejecting||pointer_active||pointer_cursor.contact_count);
 pointer_gesture_reset(&pointer_motion,true);pointer_active=false;
 if(deliver&&had){
  neutral=false;
  pointer_packet=(risc_scene_pointer_event_v1){.struct_size=sizeof(pointer_packet),.kind=RISC_POINTER_RESET};
  pointer_packet.intent.struct_size=sizeof(pointer_packet.intent);
  pointer_packet.intent.event=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.document_revision=document.revision};
  pointer_pending=true;
 }
}
static bool pointer_boundary(void){
 if(!pointer_enabled)return false;
 pointer_reset(true);
 raw_head=raw_count=0;last_sequence=capture_sequence;pointer_sync();
 memcpy(dispatched_contacts,captured_contacts,sizeof(dispatched_contacts));dispatched_count=touch_state.contact_count;
 neutral=false;return pointer_pending;
}
static const hit *pointer_hit(void){
 if(!pointer_enabled||!model->components)return NULL;
 for(unsigned i=0;i<visible.count;i++)if(visible.items[i].kind==HIT_POINTER)return &visible.items[i];
 return NULL;
}
static pointer_identity pointer_current_identity(void){
 const hit *h=pointer_hit();pointer_identity id={0};
 if(h)id=(pointer_identity){document.revision,current_route(),epoch,h->node,h->x,h->y,h->w,h->h};
 return id;
}
static bool pointer_visible(void){
 pointer_identity id=pointer_current_identity();
 return id.revision&&!memcmp(&id,&pointer_presented_identity,sizeof(id));
}
static bool pointer_contains(const hit *h,int x,int y){return h&&x>=h->x&&x<h->x+h->w&&y>=h->y&&y<h->y+h->h;}
static bool pointer_topmost(int x,int y){
 for(unsigned i=visible.count;i;i--)if(pointer_contains(&visible.items[i-1],x,y))return visible.items[i-1].kind==HIT_POINTER;
 return false;
}
/* Replay validation uses raw coordinates, not the final snapshot as an
 * invented starting state. A failed batch exposes no partial pointer action. */
static bool pointer_advance(risc_touch_snapshot_v1 *s,const risc_touch_event_v1 *e){
 if(e->sequence!=s->sequence+1||e->timestamp_ms<s->timestamp_ms||
    e->kind<RISC_TOUCH_EVENT_DOWN||e->kind>RISC_TOUCH_EVENT_UP||
    e->x>=s->width||e->y>=s->height)return false;
 unsigned at=0;while(at<s->contact_count&&s->contacts[at].id!=e->id)++at;
 if(e->kind==RISC_TOUCH_EVENT_DOWN){
  if(at<s->contact_count||s->contact_count==RISC_TOUCH_MAX_CONTACTS)return false;
  ++s->contact_count;
 }else if(at==s->contact_count)return false;
 if(e->kind==RISC_TOUCH_EVENT_UP)s->contacts[at]=s->contacts[--s->contact_count];
 else s->contacts[at]=(risc_touch_contact_v1){.id=e->id,.x=e->x,.y=e->y};
 s->sequence=e->sequence;s->timestamp_ms=e->timestamp_ms;return true;
}
static bool pointer_trace_valid(void){
 risc_touch_snapshot_v1 end=pointer_cursor;uint64_t group=0;bool lifted=false;
 for(unsigned i=0;i<raw_count;i++){
  const risc_touch_event_v1 *e=&raw_queue[(raw_head+i)%RAW_QUEUE_SIZE];
  if(i==0||e->timestamp_ms!=group){group=e->timestamp_ms;lifted=false;}
  if(e->kind==RISC_TOUCH_EVENT_DOWN&&lifted)return false;
  if(e->kind==RISC_TOUCH_EVENT_UP)lifted=true;
  if(!pointer_advance(&end,e))return false;
 }
 if(end.sequence!=touch_state.sequence||end.contact_count!=touch_state.contact_count||end.buttons!=touch_state.buttons)return false;
 for(unsigned i=0;i<end.contact_count;i++){
  unsigned j=0;while(j<touch_state.contact_count&&touch_state.contacts[j].id!=end.contacts[i].id)++j;
  if(j==touch_state.contact_count||end.contacts[i].x!=touch_state.contacts[j].x||end.contacts[i].y!=touch_state.contacts[j].y)return false;
 }
 return true;
}
static bool pointer_prepare(void){
 uint64_t now=clock_api->monotonic_ms(clock_api->context);
 if(!alive())return false;
 pointer_gap=pointer_clock_seen&&(now<pointer_last||now-pointer_last>100);pointer_last=now;pointer_clock_seen=true;
 if(pointer_gap)pointer_reset(true);
 if(!pointer_trace_valid()){
  pointer_reset(true);hold_cancel(RISC_HOLD_REASON_AMBIGUOUS);
  raw_head=raw_count=0;last_sequence=capture_sequence;pointer_sync();
  memcpy(dispatched_contacts,captured_contacts,sizeof(dispatched_contacts));dispatched_count=touch_state.contact_count;
  contact=false;top_pending=false;neutral=false;return true;
 }
 if(!raw_count&&!pointer_cursor.contact_count&&!pointer_cursor.buttons){
  const hit *h=pointer_hit();
  pointer_rejecting=false;
  if(h)(void)pointer_gesture_sample_bounds(&pointer_motion,&pointer_cursor,(uint32_t)now,h->x,h->y,h->x+h->w,h->y+h->h);
 }
 return true;
}
static bool pointer_event(const risc_touch_event_v1 *e){
 bool before=pointer_cursor.contact_count!=0;
 bool release_moved=false;
 if(e->kind==RISC_TOUCH_EVENT_UP)for(unsigned i=0;i<pointer_cursor.contact_count;i++)if(pointer_cursor.contacts[i].id==e->id){
  int ox=0,oy=0,nx=0,ny=0;
  if(!touch_point(pointer_cursor.contacts[i].x,pointer_cursor.contacts[i].y,&ox,&oy)||!touch_point(e->x,e->y,&nx,&ny)||((ox!=nx||oy!=ny)&&(pointer_motion.dragging||pointer_abs(nx-pointer_motion.sx[i])>POINTER_TAP_SLOP*(width>=480?2:1)||pointer_abs(ny-pointer_motion.sy[i])>POINTER_TAP_SLOP*(width>=480?2:1))))release_moved=true;
 }
 if(!pointer_advance(&pointer_cursor,e)){pointer_reset(true);return true;}
 /* Held controls must see and cancel competing contacts themselves. */
 if(hold_active||hold_pending||hold_nav_buttons||contact)return false;
 int x=0,y=0;bool mapped=touch_point(e->x,e->y,&x,&y);
 bool start=e->kind==RISC_TOUCH_EVENT_DOWN&&mapped&&pointer_topmost(x,y);
 if(!pointer_active&&!pointer_rejecting&&!start)return false;
 const hit *h=pointer_hit();
 if(start&&!pointer_active&&!pointer_rejecting){
  if(before||!neutral||pointer_gap||!pointer_visible()||!view_current()||handoff_waiting)pointer_rejecting=true;
  else pointer_active=true;
 }
 last_sequence=e->sequence;track_contact(dispatched_contacts,&dispatched_count,e);
 contact=false;top_pending=false;
 if(pointer_rejecting||release_moved||!h||!mapped||!pointer_visible()||!view_current()||handoff_waiting||pointer_cursor.contact_count>2){
  pointer_reset(true);
  if(!pointer_cursor.contact_count){pointer_rejecting=false;neutral=true;pointer_gesture_reset(&pointer_motion,false);}
  return true;
 }
 /* A controller report may enqueue MOVE before another contact's DOWN.
  * Recognize only its complete same-timestamp topology. */
 if(raw_count&&raw_queue[raw_head].timestamp_ms==e->timestamp_ms)return true;
 risc_touch_snapshot_v1 sample=pointer_cursor;sample.width=(uint16_t)width;sample.height=(uint16_t)height;
 for(unsigned i=0;i<sample.contact_count;i++){
  if(!touch_point(sample.contacts[i].x,sample.contacts[i].y,&x,&y)){pointer_reset(true);return true;}
  sample.contacts[i].x=(uint16_t)x;sample.contacts[i].y=(uint16_t)y;
 }
 pointer_gesture_output out=pointer_gesture_sample_bounds(&pointer_motion,&sample,(uint32_t)e->timestamp_ms,h->x,h->y,h->x+h->w,h->y+h->h);
 if(!sample.contact_count){pointer_active=false;neutral=true;}
 if(out.dx||out.dy||out.wheel_x||out.wheel_y||out.click||out.button_changed){
  if(event_serial==UINT64_MAX){pointer_active=true;pointer_reset(true);hold_fault=true;return true;}
  if(pointer_pending&&pointer_packet.kind==RISC_POINTER_REPORT&&!pointer_packet.click&&!out.click&&pointer_packet.buttons==out.buttons){
   pointer_packet.dx+=out.dx;pointer_packet.dy+=out.dy;pointer_packet.wheel_x+=out.wheel_x;pointer_packet.wheel_y+=out.wheel_y;return true;
  }
  pointer_packet=(risc_scene_pointer_event_v1){.struct_size=sizeof(pointer_packet),.kind=RISC_POINTER_REPORT,
   .dx=out.dx,.dy=out.dy,.wheel_x=out.wheel_x,.wheel_y=out.wheel_y,.buttons=out.buttons,.click=out.click};
  pointer_packet.intent.struct_size=sizeof(pointer_packet.intent);
  int at=node_index(&document,h->node);
  pointer_packet.intent.event=(risc_scene_event_v1){.struct_size=sizeof(risc_scene_event_v1),.document_revision=document.revision,.node=h->node,.action=at>=0?document.nodes[at].action:0,.sequence=++event_serial};
  pointer_pending=true;
 }
 return true;
}

static bool pointer_can_coalesce(void){
 return pointer_pending&&pointer_packet.kind==RISC_POINTER_REPORT&&!pointer_packet.click&&raw_count&&raw_queue[raw_head].kind==RISC_TOUCH_EVENT_MOVE&&!(pointer_motion.drag_candidate&&!pointer_motion.dragging);
}
