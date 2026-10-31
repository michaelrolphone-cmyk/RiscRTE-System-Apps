#pragma once
#include "canvas_frames.h"
#include "RiscTouchV1.h"
typedef struct {
 canvas_frames frames;
 risc_canvas_open_v1 request;
 const risc_touch_api_v1 *touch;
 const risc_input_navigation_api_v1 *navigation;
 uint64_t subscription,input_sequence,raw_sequence;
 uint32_t rotation,touch_rotation,preferred_format;
 risc_input_navigation_frame_v1 previous_navigation;
 bool claimed,input_gap,gap_emitted,geometry_changed;
 bool captured[UINT8_MAX+1u];
 risc_canvas_api_v1 api;
} canvas_provider;
static inline int32_t canvas_provider_geometry_refresh(canvas_provider*p){
 risc_display_info_v1 info={0};canvas_mapping m;
 bool read=p->frames.display->get_info(p->frames.display->context,&info);
 if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;
 if(!read)return RISC_CANVAS_UNAVAILABLE;
 if(!canvas_mapping_open(&m,info.width,info.height,p->rotation,info.safe_insets.left,info.safe_insets.top,info.safe_insets.right,info.safe_insets.bottom,&p->request))return RISC_CANVAS_UNSUPPORTED;
 uint64_t stride=p->request.encoding==RISC_CANVAS_RGBA8?(uint64_t)m.width*4:((uint64_t)m.width+7)/8;
 if(stride>UINT32_MAX||m.height>SIZE_MAX/stride)return RISC_CANVAS_UNSUPPORTED;
 if(memcmp(&m,&p->frames.mapping,sizeof(m))){
  if(p->frames.revision==UINT32_MAX)return RISC_CANVAS_UNAVAILABLE;
  p->frames.mapping=m;p->frames.revision++;p->frames.stride=(uint32_t)stride;p->frames.bytes=(size_t)stride*m.height;p->input_gap=true;p->gap_emitted=false;p->geometry_changed=true;
 }
 return RISC_CANVAS_OK;
}
static int32_t canvas_provider_input_acquire(canvas_provider*p){
 if(p->touch&&!p->subscription){
  const risc_touch_api_v1*t=p->touch;
  if(t->api_version!=1||t->struct_size<sizeof(*t)||!t->subscribe||!t->unsubscribe||!t->poll||!t->next||!t->snapshot)return RISC_CANVAS_UNAVAILABLE;
  p->subscription=t->subscribe(t->context);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;if(!p->subscription)return RISC_CANVAS_UNAVAILABLE;
 }
 if(p->navigation&&!p->claimed){
  const risc_input_navigation_api_v1*n=p->navigation;
  if(n->api_version!=1||n->struct_size<sizeof(*n)||!n->poll||!n->foreground||!n->reset)return RISC_CANVAS_UNAVAILABLE;
  risc_input_foreground_v1 claim={"input.touch.raw",1};
  p->claimed=true; /* Attempted foreground ownership also requires rollback. */
  bool claimed=n->foreground(n->context,p->touch?&claim:NULL,p->touch?1:0);
  if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;
  if(!claimed)return RISC_CANVAS_UNAVAILABLE;
  bool reset=n->reset(n->context);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;if(!reset)return RISC_CANVAS_UNAVAILABLE;
 }
 return RISC_CANVAS_OK;
}
static int32_t canvas_provider_input_release(canvas_provider*p){
 if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;
 if(p->subscription){
  bool ok=p->touch->unsubscribe(p->touch->context,p->subscription);
  if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;
  if(!ok){p->frames.retained=true;return RISC_CANVAS_RETAINED;}
 }
 p->subscription=0;
 if(p->claimed){
  bool ok=p->navigation->foreground(p->navigation->context,NULL,0);
  if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;
  if(!ok){p->frames.retained=true;return RISC_CANVAS_RETAINED;}
  ok=p->navigation->reset(p->navigation->context);
  if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;
  if(!ok){p->frames.retained=true;return RISC_CANVAS_RETAINED;}
 }
 p->claimed=false;return RISC_CANVAS_OK;
}
static int32_t canvas_provider_open(void*ctx,const risc_canvas_open_v1*request,uint64_t*out){
 canvas_provider*p=ctx;canvas_frames*c=&p->frames;
 if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;
 if(!request||request->struct_size<sizeof(*request)||!out)return RISC_CANVAS_INVALID;
 if(c->session)return RISC_CANVAS_BUSY; /* Single physical foreground owner. */
 if(request->encoding!=RISC_CANVAS_MONO1&&request->encoding!=RISC_CANVAS_RGBA8)return RISC_CANVAS_UNSUPPORTED;
 const risc_display_output_api_v1*d=c->display;
 if(!d||d->api_version!=1||d->struct_size<offsetof(risc_display_output_api_v1,present_status)+sizeof(d->present_status)||!d->get_info||!d->acquire||!d->release||!d->submit||!d->present_status)return RISC_CANVAS_UNAVAILABLE;
 risc_display_info_v1 info={0};bool read=d->get_info(d->context,&info);if(!canvas_frames_alive(c))return RISC_CANVAS_RETAINED;if(!read)return RISC_CANVAS_UNAVAILABLE;
 uint32_t format=p->preferred_format?p->preferred_format:((info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565))?RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1);
 if(format!=RISC_DISPLAY_FORMAT_MONO1&&format!=RISC_DISPLAY_FORMAT_RGB565)return RISC_CANVAS_UNSUPPORTED;
 if(!(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(format)))return RISC_CANVAS_UNSUPPORTED;
 if(request->encoding==RISC_CANVAS_RGBA8&&format!=RISC_DISPLAY_FORMAT_RGB565)return RISC_CANVAS_UNSUPPORTED;
 p->request=*request;int32_t rc=canvas_provider_geometry_refresh(p);if(rc)return rc;
 if(c->serial==UINT64_MAX)return RISC_CANVAS_UNAVAILABLE;
 rc=canvas_provider_input_acquire(p);
 if(rc){int32_t cleanup=canvas_provider_input_release(p);return cleanup?cleanup:rc;}
 c->encoding=request->encoding;c->format=format;c->session=++c->serial;c->quiesced=false;
 memset(p->captured,0,sizeof(p->captured));memset(&p->previous_navigation,0,sizeof(p->previous_navigation));p->raw_sequence=0;p->geometry_changed=false;p->input_gap=true;p->gap_emitted=false;*out=c->session;return RISC_CANVAS_OK;
}
static int32_t canvas_provider_geometry(void*ctx,uint64_t session,risc_canvas_geometry_v1*out){
 canvas_provider*p=ctx;int32_t rc=canvas_frames_fence(&p->frames,session);if(rc)return rc;
 if(!out||out->struct_size<sizeof(*out))return RISC_CANVAS_INVALID;
 rc=canvas_provider_geometry_refresh(p);if(rc)return rc;
 canvas_mapping*m=&p->frames.mapping;
 uint32_t x=m->output_x<0?(uint32_t)((uint64_t)(-m->output_x)*m->width/m->output_width):0;
 uint32_t y=m->output_y<0?(uint32_t)((uint64_t)(-m->output_y)*m->height/m->output_height):0;
 uint32_t w=m->output_width>m->safe.width?(uint32_t)((uint64_t)m->safe.width*m->width/m->output_width):m->width;
 uint32_t h=m->output_height>m->safe.height?(uint32_t)((uint64_t)m->safe.height*m->height/m->output_height):m->height;
 *out=(risc_canvas_geometry_v1){sizeof(*out),p->frames.revision,m->width,m->height,{(int32_t)x,(int32_t)y,w,h}};return RISC_CANVAS_OK;
}
static int32_t canvas_provider_begin(void*ctx,uint64_t s,risc_canvas_frame_v1*out){canvas_provider*p=ctx;int32_t rc=canvas_frames_fence(&p->frames,s);if(rc)return rc;rc=canvas_provider_geometry_refresh(p);return rc?rc:canvas_frames_begin(&p->frames,s,out);}
static int32_t canvas_provider_submit(void*ctx,uint64_t s,uint64_t lease,uint64_t*out){canvas_provider*p=ctx;int32_t rc=canvas_frames_fence(&p->frames,s);if(rc)return rc;rc=canvas_provider_geometry_refresh(p);return rc?rc:canvas_frames_submit(&p->frames,s,lease,out);}
static int32_t canvas_provider_cancel(void*ctx,uint64_t s,uint64_t lease){return canvas_frames_cancel(&((canvas_provider*)ctx)->frames,s,lease);}
static int32_t canvas_provider_poll(void*ctx,uint64_t s,uint64_t id,risc_canvas_receipt_v1*out){return canvas_frames_poll(&((canvas_provider*)ctx)->frames,s,id,out);}
static int32_t canvas_provider_forget(void*ctx,uint64_t s,uint64_t id){return canvas_frames_forget(&((canvas_provider*)ctx)->frames,s,id);}
static int32_t canvas_provider_quiesce(void*ctx,uint64_t s){
 canvas_provider*p=ctx;canvas_frames*c=&p->frames;int32_t rc=canvas_frames_fence(c,s);if(rc)return rc;if(c->lease)return RISC_CANVAS_BUSY;
 for(canvas_receipt_node*n=c->receipts;n;n=n->next){risc_canvas_receipt_v1 receipt={.struct_size=sizeof(receipt)};rc=canvas_frames_poll(c,s,n->id,&receipt);if(rc)return rc;if(!canvas_receipt_settled(receipt.state))return RISC_CANVAS_AGAIN;}
 rc=canvas_provider_input_release(p);if(rc)return rc;c->quiesced=true;return RISC_CANVAS_OK;
}
static int32_t canvas_provider_resume(void*ctx,uint64_t s){canvas_provider*p=ctx;int32_t rc=canvas_frames_fence(&p->frames,s);if(rc)return rc;rc=canvas_provider_geometry_refresh(p);if(rc)return rc;rc=canvas_provider_input_acquire(p);if(rc){int32_t cleanup=canvas_provider_input_release(p);return cleanup?cleanup:rc;}p->input_gap=true;p->gap_emitted=false;p->frames.quiesced=false;return RISC_CANVAS_OK;}
static int32_t canvas_provider_close(void*ctx,uint64_t s){
 canvas_provider*p=ctx;int32_t rc=canvas_provider_quiesce(ctx,s);if(rc)return rc;
 p->subscription=0;while(p->frames.receipts){canvas_receipt_node*n=p->frames.receipts;p->frames.receipts=n->next;free(n);}p->frames.session=0;return RISC_CANVAS_OK;
}
/* Reuse scene host's independent raw-touch rotation policy. Display rotation
 * describes raster orientation; raw touch orientation is a separate provider input. */
static bool canvas_provider_touch_point(canvas_provider*p,uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t*px,uint32_t*py){
 if(!w||!h||x>=w||y>=h)return false;
 uint32_t tx=x,ty=y,tw=w,th=h;
 switch(p->touch_rotation){case 1:tx=h-1-y;ty=x;tw=h;th=w;break;case 2:tx=w-1-x;ty=h-1-y;break;case 3:tx=y;ty=w-1-x;tw=h;th=w;break;default:break;}
 canvas_mapping*m=&p->frames.mapping;uint32_t lw=m->rotation%2?m->physical_height:m->physical_width,lh=m->rotation%2?m->physical_width:m->physical_height;
 uint32_t lx=(uint64_t)tx*lw/tw,ly=(uint64_t)ty*lh/th;
 canvas_mapping_physical(m,lx,ly,px,py);return true;
}
static bool canvas_provider_touch_admit(canvas_provider*p,uint32_t x,uint32_t y,uint32_t w,uint32_t h){
 uint32_t px,py;if(!canvas_provider_touch_point(p,x,y,w,h,&px,&py))return false;
 return canvas_mapping_admit(&p->frames.mapping,px,py,p->frames.mapping.physical_width,p->frames.mapping.physical_height);
}
static bool canvas_provider_touch_map(canvas_provider*p,uint32_t x,uint32_t y,uint32_t w,uint32_t h,int32_t*cx,int32_t*cy){
 uint32_t px,py;if(!canvas_provider_touch_point(p,x,y,w,h,&px,&py))return false;
 return canvas_mapping_contact(&p->frames.mapping,px,py,p->frames.mapping.physical_width,p->frames.mapping.physical_height,cx,cy);
}
static int32_t canvas_provider_contacts(void*ctx,uint64_t s,risc_canvas_event_v1*out,size_t capacity,size_t*count,uint64_t*sequence){
 canvas_provider*p=ctx;int32_t rc=canvas_frames_fence(&p->frames,s);if(rc)return rc;
 if(!count||!sequence||(!out&&capacity))return RISC_CANVAS_INVALID;
 if(p->frames.quiesced)return RISC_CANVAS_BUSY;
 rc=canvas_provider_geometry_refresh(p);if(rc)return rc;
 risc_touch_snapshot_v1 snap={0};
 if(p->touch){bool ok=p->touch->snapshot(p->touch->context,&snap);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;if(!ok||!snap.width||!snap.height||snap.contact_count>RISC_TOUCH_MAX_CONTACTS)return RISC_CANVAS_UNAVAILABLE;}
 if(p->touch&&snap.sequence<p->raw_sequence)return RISC_CANVAS_UNAVAILABLE;
 bool seen[UINT8_MAX+1u]={0};
 risc_canvas_event_v1 mapped[RISC_TOUCH_MAX_CONTACTS];size_t n=0;bool captured[UINT8_MAX+1u]={0};
 for(unsigned i=0;i<snap.contact_count;i++){
  risc_touch_contact_v1*c=&snap.contacts[i];if(seen[c->id])return RISC_CANVAS_UNAVAILABLE;seen[c->id]=true;
  if(!p->captured[c->id]&&!canvas_provider_touch_admit(p,c->x,c->y,snap.width,snap.height))continue;
  int32_t x,y;if(!canvas_provider_touch_map(p,c->x,c->y,snap.width,snap.height,&x,&y))return RISC_CANVAS_UNAVAILABLE;
  mapped[n++]=(risc_canvas_event_v1){.struct_size=sizeof(*mapped),.kind=RISC_CANVAS_CONTACT,.geometry_revision=p->frames.revision,.phase=RISC_CANVAS_MOVE,.contact=c->id,.x=x,.y=y};captured[c->id]=true;
 }
 if(capacity<n){*count=n;return RISC_CANVAS_AGAIN;}
 if(p->input_sequence==UINT64_MAX)return RISC_CANVAS_UNAVAILABLE;
 uint64_t cut=p->input_sequence+1;
 for(size_t i=0;i<n;i++){if(out[i].struct_size<sizeof(*out))return RISC_CANVAS_INVALID;mapped[i].sequence=cut;}
 if(p->input_gap&&p->navigation){
  bool ok=p->navigation->reset(p->navigation->context);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;if(!ok){p->frames.retained=true;return RISC_CANVAS_RETAINED;}
  memset(&p->previous_navigation,0,sizeof(p->previous_navigation));
 }
 for(size_t i=0;i<n;i++)out[i]=mapped[i];
 *count=n;*sequence=cut;p->input_sequence=cut;p->raw_sequence=snap.sequence;memcpy(p->captured,captured,sizeof(captured));p->input_gap=false;p->gap_emitted=false;return RISC_CANVAS_OK;
}
static int32_t canvas_provider_next(void*ctx,uint64_t s,risc_canvas_event_v1*out){
 canvas_provider*p=ctx;int32_t rc=canvas_frames_fence(&p->frames,s);if(rc)return rc;
 if(!out||out->struct_size<sizeof(*out))return RISC_CANVAS_INVALID;
 if(p->frames.quiesced)return RISC_CANVAS_BUSY;
 rc=canvas_provider_geometry_refresh(p);if(rc)return rc;
 if(p->input_sequence==UINT64_MAX)return RISC_CANVAS_UNAVAILABLE;
 risc_canvas_event_v1 event={.struct_size=sizeof(event),.geometry_revision=p->frames.revision};
 if(p->geometry_changed){event.kind=RISC_CANVAS_GEOMETRY_CHANGED;p->geometry_changed=false;}
 else if(p->input_gap){if(p->gap_emitted)return RISC_CANVAS_AGAIN;event.kind=RISC_CANVAS_INPUT_GAP;p->gap_emitted=true;}
 else if(p->navigation){
  risc_input_navigation_frame_v1 nav={0};
  bool polled=p->navigation->poll(p->navigation->context,&nav);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;if(!polled){p->input_gap=true;p->gap_emitted=false;return canvas_provider_next(ctx,s,out);}
  if(nav.buttons!=p->previous_navigation.buttons||nav.pressed||nav.released){
   p->previous_navigation=nav;event.kind=RISC_CANVAS_NAVIGATION;event.navigation=nav;
   event.sequence=++p->input_sequence;*out=event;return RISC_CANVAS_OK;
  }
 }
 if(!event.kind&&p->touch){
  bool ok=p->touch->poll(p->touch->context,RISC_TOUCH_QUEUE_LENGTH);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;risc_touch_event_v1 e={0};
  int32_t got=p->touch->next(p->touch->context,p->subscription,&e);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;
  if(!ok||got<0){p->input_gap=true;p->gap_emitted=false;return canvas_provider_next(ctx,s,out);}
  if(!got)return RISC_CANVAS_IDLE;
  if(!e.sequence){p->input_gap=true;p->gap_emitted=false;return canvas_provider_next(ctx,s,out);}
  if(e.sequence<=p->raw_sequence)return RISC_CANVAS_IDLE;
  if(p->raw_sequence&&e.sequence!=p->raw_sequence+1){p->input_gap=true;p->gap_emitted=false;return canvas_provider_next(ctx,s,out);}
  p->raw_sequence=e.sequence;
  if(e.kind==RISC_TOUCH_EVENT_BUTTON_DOWN&&e.id==0){event.kind=RISC_CANVAS_CONTROLS_REQUEST;}
  else if(e.kind>=RISC_TOUCH_EVENT_BUTTON_DOWN)return RISC_CANVAS_IDLE;
  else{
   
   risc_touch_snapshot_v1 snap={0};bool snapped=p->touch->snapshot(p->touch->context,&snap);if(!canvas_frames_alive(&p->frames))return RISC_CANVAS_RETAINED;if(!snapped){p->input_gap=true;return RISC_CANVAS_AGAIN;}
   if(e.kind<RISC_TOUCH_EVENT_DOWN||e.kind>RISC_TOUCH_EVENT_UP){p->input_gap=true;p->gap_emitted=false;return canvas_provider_next(ctx,s,out);}
   if(e.kind==RISC_TOUCH_EVENT_DOWN){if(p->captured[e.id]){p->input_gap=true;p->gap_emitted=false;return canvas_provider_next(ctx,s,out);}if(!canvas_provider_touch_admit(p,e.x,e.y,snap.width,snap.height))return RISC_CANVAS_IDLE;p->captured[e.id]=true;}
   else if(!p->captured[e.id])return RISC_CANVAS_IDLE;
   if(!canvas_provider_touch_map(p,e.x,e.y,snap.width,snap.height,&event.x,&event.y)){p->input_gap=true;return RISC_CANVAS_AGAIN;}
   event.kind=RISC_CANVAS_CONTACT;event.contact=e.id;event.phase=e.kind;
   if(e.kind==RISC_TOUCH_EVENT_UP)p->captured[e.id]=false;
  }
 }else if(!event.kind)return RISC_CANVAS_IDLE;
 event.sequence=++p->input_sequence;*out=event;return RISC_CANVAS_OK;
}
static inline void canvas_provider_init(canvas_provider*p,const risc_display_output_api_v1*display,const risc_touch_api_v1*touch,uint32_t rotation){
 memset(p,0,sizeof(*p));p->frames.display=display;p->touch=touch;p->rotation=rotation;
 p->api=(risc_canvas_api_v1){1,sizeof(p->api),RISC_CANVAS_TAG,p,canvas_provider_open,canvas_provider_geometry,canvas_provider_begin,canvas_provider_submit,canvas_provider_cancel,canvas_provider_poll,canvas_provider_forget,canvas_provider_next,canvas_provider_contacts,canvas_provider_quiesce,canvas_provider_resume,canvas_provider_close};
}
