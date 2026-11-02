#pragma once
/* Shared logical raster adapter (adaptive RGB565 or one-bit ink). Physical format, rotation, quantization
 * and refresh ownership stay in ui.canvas. This is not a device selector.
 * One frame and one receipt at a time, matching the Home renderer's cadence. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "RiscCanvasV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscDisplayHistoryV1.h"
#include "RiscTouchV1.h"
typedef struct {
 const risc_canvas_api_v1 *canvas;
 uint64_t session,lease,receipt;
 risc_canvas_geometry_v1 geometry;
 union {risc_display_output_api_v1 display;risc_display_output_api_v1_history history;};
 const risc_canvas_presentation_v1 *presentation;
 risc_canvas_frame_v1 frame;
 uint32_t format,encoding;
 bool retained,quiesced;
 int32_t last_result;
 void (*on_retained)(void*);void *retained_context;
 uint64_t contact_identity;
 uint8_t contact_tag;
 bool contact_known;
 bool (*brightness)(void*,uint16_t,uint16_t);
 void *brightness_context;
} home_canvas_surface;
static inline void home_canvas_retain(home_canvas_surface*s){
 if(s->retained)return;
 s->retained=true;
 if(s->on_retained)s->on_retained(s->retained_context);
}
static inline bool home_canvas_result(home_canvas_surface*s,int32_t rc){
 s->last_result=rc;
 if(rc==RISC_CANVAS_RETAINED || rc<RISC_CANVAS_UNSUPPORTED || rc>RISC_CANVAS_AGAIN)home_canvas_retain(s);
 return rc==RISC_CANVAS_OK;
}
static inline bool home_canvas_geometry(home_canvas_surface*s){
 if(s->retained||!s->session)return false;
 risc_canvas_geometry_v1 g={.struct_size=sizeof(g)};
 if(!home_canvas_result(s,s->canvas->geometry(s->canvas->context,s->session,&g)))return false;
 if(g.struct_size<sizeof(g)||!g.revision||!g.width||!g.height||g.width>UINT16_MAX||g.height>UINT16_MAX){home_canvas_retain(s);return false;}
 s->geometry=g;return true;
}
static bool home_canvas_info(void*ctx,risc_display_info_v1*out){
 home_canvas_surface*s=ctx;if(!out||!home_canvas_geometry(s))return false;
 *out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),
  .width=s->geometry.width,.height=s->geometry.height,
  .supported_formats=RISC_DISPLAY_FORMAT_BIT(s->format),
  .preferred_format=s->format,
  .supported_rotations=RISC_DISPLAY_ROTATION_0,
  .flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|
   (s->brightness?RISC_DISPLAY_INFO_BRIGHTNESS:0)|
   (s->presentation?RISC_DISPLAY_INFO_PARTIAL_DAMAGE:0)|
   (s->presentation&&(s->presentation->flags&RISC_CANVAS_PRESENT_RETAINS_IMAGE)?RISC_DISPLAY_INFO_RETAINS_IMAGE:0)|
   (s->presentation&&(s->presentation->flags&RISC_CANVAS_PRESENT_CLEAN_SUPPORTED)?RISC_DISPLAY_INFO_CLEAN_PRESENT:0)};
 return true;
}
static bool home_canvas_acquire(void*ctx,uint32_t format,risc_display_surface_v1*out){
 home_canvas_surface*s=ctx;
 if(!out||s->retained||!s->session||s->lease||s->receipt||format!=s->format)return false;
 if(s->quiesced){
  if(!home_canvas_result(s,s->canvas->resume(s->canvas->context,s->session)))return false;
  s->quiesced=false;
 }
 if(!home_canvas_geometry(s))return false;
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};
 if(!home_canvas_result(s,s->canvas->begin_frame(s->canvas->context,s->session,&f)))return false;
 /* Malformed ownership cannot be released speculatively. */
 if(f.struct_size<sizeof(f)||!f.lease||!f.pixels||f.encoding!=s->encoding||
    f.geometry_revision!=s->geometry.revision||f.width!=s->geometry.width||f.height!=s->geometry.height||
    f.stride<(s->format==RISC_DISPLAY_FORMAT_MONO1?((uint64_t)f.width+7)/8:(uint64_t)f.width*2)||f.byte_count>UINT32_MAX||
    f.height>f.byte_count/f.stride||(s->format==RISC_DISPLAY_FORMAT_RGB565&&((f.stride&1u)||((uintptr_t)f.pixels&1u)))){home_canvas_retain(s);return false;}
 s->lease=f.lease;s->frame=f;
 if(s->format==RISC_DISPLAY_FORMAT_MONO1)for(size_t i=0;i<f.byte_count;i++)((unsigned char*)f.pixels)[i]^=255u;
 *out=(risc_display_surface_v1){f.lease,f.pixels,f.width,f.height,f.stride,(uint32_t)f.byte_count,s->format};
 return true;
}
static void home_canvas_release(void*ctx,risc_display_frame_v1 lease){
 home_canvas_surface*s=ctx;if(s->retained||!lease||lease!=s->lease)return;
 if(home_canvas_result(s,s->canvas->cancel_frame(s->canvas->context,s->session,lease))){s->lease=0;s->frame=(risc_canvas_frame_v1){0};}
 else home_canvas_retain(s); /* void facade cannot report unproved cancellation */
}
static bool home_canvas_submit(void*ctx,risc_display_frame_v1 lease,const risc_display_rect_v1*damage,size_t count,
 const risc_display_present_options_v1*options,risc_display_present_token_v1*out){
 home_canvas_surface*s=ctx;
 if(!out||s->retained||!lease||lease!=s->lease||s->receipt||count>RISC_DISPLAY_MAX_DAMAGE_RECTS||(count&&!damage))return false;
 /* Do not silently discard an unsupported semantic presentation request. */
 if(options&&(options->intent>RISC_DISPLAY_PRESENT_CLEAN||options->queue_policy!=RISC_DISPLAY_QUEUE_FIFO||options->reserved))return false;
 if(!s->presentation&&options&&options->intent!=RISC_DISPLAY_PRESENT_DEFAULT)return false;
 for(size_t i=0;i<count;i++)if(damage[i].x<0||damage[i].y<0||!damage[i].width||!damage[i].height||
  (uint64_t)(uint32_t)damage[i].x+damage[i].width>s->geometry.width||
  (uint64_t)(uint32_t)damage[i].y+damage[i].height>s->geometry.height)return false;
 uint64_t receipt=0;
 risc_canvas_rect_v1 logical_damage[RISC_DISPLAY_MAX_DAMAGE_RECTS];
 for(size_t i=0;i<count;i++)logical_damage[i]=(risc_canvas_rect_v1){damage[i].x,damage[i].y,damage[i].width,damage[i].height};
 risc_canvas_present_v1 intent={sizeof(intent),options?options->intent:RISC_CANVAS_PRESENT_DEFAULT,logical_damage,count};
 if(s->format==RISC_DISPLAY_FORMAT_MONO1)for(size_t i=0;i<s->frame.byte_count;i++)((unsigned char*)s->frame.pixels)[i]^=255u;
 int32_t rc=s->presentation?s->presentation->submit(s->presentation->context,s->session,lease,&intent,&receipt):
  s->canvas->submit(s->canvas->context,s->session,lease,&receipt);
 if(!home_canvas_result(s,rc)){
  if(!s->retained&&s->format==RISC_DISPLAY_FORMAT_MONO1)for(size_t i=0;i<s->frame.byte_count;i++)((unsigned char*)s->frame.pixels)[i]^=255u;
  return false;
 }
 if(!receipt){home_canvas_retain(s);return false;}
 s->lease=0;s->frame=(risc_canvas_frame_v1){0};s->receipt=receipt;*out=receipt;return true;
}
static bool home_canvas_status(void*ctx,risc_display_present_token_v1 receipt,risc_display_present_status_v1*out){
 home_canvas_surface*s=ctx;if(!out||s->retained||!receipt||receipt!=s->receipt)return false;
 risc_canvas_receipt_v1 r={.struct_size=sizeof(r)};
 if(!home_canvas_result(s,s->canvas->poll(s->canvas->context,s->session,receipt,&r)))return false;
 if(r.struct_size<sizeof(r)||r.receipt!=receipt||r.state<RISC_CANVAS_QUEUED||r.state>RISC_CANVAS_FAILED){home_canvas_retain(s);return false;}
 if(r.state>=RISC_CANVAS_COMPLETE){
  if(!home_canvas_result(s,s->canvas->forget_receipt(s->canvas->context,s->session,receipt)))return false;
  s->receipt=0;
 }
 *out=(risc_display_present_status_v1){.state=(uint8_t)r.state};return true;
}
static bool home_canvas_wait(void*ctx,risc_display_present_token_v1 receipt,uint32_t timeout,risc_display_present_status_v1*out){
 (void)timeout;return home_canvas_status(ctx,receipt,out); /* bounded nonblocking poll */
}
static bool home_canvas_brightness(void*ctx,uint16_t value,uint16_t maximum){
 home_canvas_surface*s=ctx;return !s->retained&&s->brightness&&s->brightness(s->brightness_context,value,maximum);
}
static bool home_canvas_seed_previous(void*ctx,risc_display_frame_v1 lease){
 home_canvas_surface*s=ctx;
 if(s->retained||!lease||lease!=s->lease||!s->presentation||
  !(s->presentation->flags&RISC_CANVAS_PRESENT_HISTORY_SUPPORTED))return false;
 if(s->format==RISC_DISPLAY_FORMAT_MONO1)for(size_t i=0;i<s->frame.byte_count;i++)((unsigned char*)s->frame.pixels)[i]^=255u;
 bool ok=home_canvas_result(s,s->presentation->seed_previous(s->presentation->context,s->session,lease));
 if(!s->retained&&s->format==RISC_DISPLAY_FORMAT_MONO1)for(size_t i=0;i<s->frame.byte_count;i++)((unsigned char*)s->frame.pixels)[i]^=255u;
 return ok;
}
static inline bool home_canvas_open_content_owned(home_canvas_surface*s,const risc_canvas_api_v1*api,
 uint32_t logical_format,bool presentation_required,bool(*brightness)(void*,uint16_t,uint16_t),void*context,
 void(*retain)(void*),void*retain_context){
 if(!s||!api||(logical_format!=RISC_DISPLAY_FORMAT_MONO1&&logical_format!=RISC_DISPLAY_FORMAT_RGB565))return false;
 *s=(home_canvas_surface){.canvas=api,.format=logical_format,
  .encoding=logical_format==RISC_DISPLAY_FORMAT_MONO1?RISC_CANVAS_MONO1:RISC_CANVAS_RGB565_ADAPTIVE,
  .brightness=brightness,.brightness_context=context,.on_retained=retain,.retained_context=retain_context};
 risc_canvas_open_presentation_v1 request={.base={sizeof(request),s->encoding,RISC_CANVAS_RESPONSIVE,0,0},
  .presentation_tag=RISC_CANVAS_PRESENTATION_TAG,.presentation_out=&s->presentation};
 if(!presentation_required)request.base.struct_size=sizeof(request.base);
 if(!home_canvas_result(s,api->open(api->context,&request.base,&s->session)))return false;
 if(!s->session){home_canvas_retain(s);return false;}
 if(presentation_required&&(!s->presentation||
  !risc_canvas_presentation_get_v1(s->presentation,s->presentation->struct_size))){
  if(!home_canvas_result(s,api->close(api->context,s->session))){home_canvas_retain(s);return false;}
  s->session=0;s->last_result=RISC_CANVAS_UNSUPPORTED;return false;
 }
 if(!home_canvas_geometry(s))return false;
 s->display=(risc_display_output_api_v1){1,sizeof(s->display),s,home_canvas_info,home_canvas_acquire,
  home_canvas_release,home_canvas_submit,home_canvas_status,home_canvas_wait,home_canvas_brightness};
 if(s->presentation&&(s->presentation->flags&RISC_CANVAS_PRESENT_HISTORY_SUPPORTED)){
  s->history.base.struct_size=sizeof(s->history);s->history.extension_tag=RISC_DISPLAY_HISTORY_TAG;
  s->history.extension_version=1;s->history.seed_previous=home_canvas_seed_previous;
 }
 return true;
}
static inline bool home_canvas_open_owned(home_canvas_surface*s,const risc_canvas_api_v1*api,
 bool(*brightness)(void*,uint16_t,uint16_t),void*context,void(*retain)(void*),void*retain_context){
 return home_canvas_open_content_owned(s,api,RISC_DISPLAY_FORMAT_RGB565,false,brightness,context,retain,retain_context);
}
static inline bool home_canvas_open(home_canvas_surface*s,const risc_canvas_api_v1*api,
 bool(*brightness)(void*,uint16_t,uint16_t),void*context){
 return home_canvas_open_owned(s,api,brightness,context,NULL,NULL);
}
static inline bool home_canvas_quiesce(home_canvas_surface*s){
 if(s->retained||s->lease||s->receipt)return false;
 if(s->quiesced)return true;
 if(!home_canvas_result(s,s->canvas->quiesce(s->canvas->context,s->session)))return false;
 s->quiesced=true;return true;
}
static inline bool home_canvas_resume(home_canvas_surface*s){
 if(s->retained)return false;
 if(s->quiesced&&!home_canvas_result(s,s->canvas->resume(s->canvas->context,s->session)))return false;
 s->quiesced=false;return home_canvas_geometry(s);
}
static inline bool home_canvas_close(home_canvas_surface*s){
 if(s->retained||s->lease||s->receipt)return false;
 if(s->session&&!home_canvas_result(s,s->canvas->close(s->canvas->context,s->session)))return false;
 *s=(home_canvas_surface){0};return true;
}

typedef struct {
 bool valid,activity,controls;
 uint8_t count,id;
 uint16_t x,y;
 risc_input_navigation_frame_v1 navigation;
} home_canvas_observation;
/* Preserve logical navigation separately; touch-only consumers must not silently
 * swallow button intents. Arbitrarily many contacts cancel single-contact
 * gestures; there is no assumed physical touch capacity in the Canvas ABI. */
static inline home_canvas_observation home_canvas_capture(home_canvas_surface*s){
 home_canvas_observation o={0};if(s->retained||!s->session)return o;
 bool gap=false;
 for(unsigned i=0;i<32;i++){
  risc_canvas_event_v1 e={.struct_size=sizeof(e)};
  int32_t rc=s->canvas->next(s->canvas->context,s->session,&e);
  if(rc==RISC_CANVAS_IDLE||rc==RISC_CANVAS_AGAIN)break;
  if(!home_canvas_result(s,rc))return o;
  if(e.struct_size<sizeof(e)||e.kind<RISC_CANVAS_CONTACT||e.kind>RISC_CANVAS_CONTROLS_REQUEST){home_canvas_retain(s);return o;}
  o.activity=true;
  if(e.kind==RISC_CANVAS_INPUT_GAP||e.kind==RISC_CANVAS_GEOMETRY_CHANGED)gap=true;
  if(e.kind==RISC_CANVAS_CONTROLS_REQUEST)o.controls=true;
  if(e.kind==RISC_CANVAS_NAVIGATION){
   o.navigation.buttons=e.navigation.buttons;
   o.navigation.pressed|=e.navigation.pressed;o.navigation.released|=e.navigation.released;
  }
 }
 if(!home_canvas_geometry(s))return o;
 size_t count=0;uint64_t sequence=0;
 int32_t rc=s->canvas->contacts(s->canvas->context,s->session,NULL,0,&count,&sequence);
 if(rc!=RISC_CANVAS_OK&&rc!=RISC_CANVAS_AGAIN){(void)home_canvas_result(s,rc);return o;}
 if(count>SIZE_MAX/sizeof(risc_canvas_event_v1))return o;
 risc_canvas_event_v1 local[2]={{.struct_size=sizeof(*local)},{.struct_size=sizeof(*local)}};
 risc_canvas_event_v1 *contacts=count>2?calloc(count,sizeof(*contacts)):local;
 if(!contacts)return o;
 size_t capacity=count>2?count:2;
 for(size_t i=0;i<capacity;i++)contacts[i].struct_size=sizeof(*contacts);
 rc=s->canvas->contacts(s->canvas->context,s->session,contacts,capacity,&count,&sequence);
 if(home_canvas_result(s,rc)&&count<=capacity){
  o.valid=!gap;o.count=(uint8_t)(count>1?2:count);
  for(size_t i=0;i<count;i++)if(contacts[i].struct_size<sizeof(*contacts)||
    contacts[i].kind!=RISC_CANVAS_CONTACT||contacts[i].geometry_revision!=s->geometry.revision||
    contacts[i].x<0||contacts[i].y<0||(uint32_t)contacts[i].x>=s->geometry.width||
    (uint32_t)contacts[i].y>=s->geometry.height)o.valid=false;
  if(count==1&&o.valid){
   if(!s->contact_known||s->contact_identity!=contacts[0].contact){
    s->contact_identity=contacts[0].contact;s->contact_tag++;s->contact_known=true;
   }
   o.id=s->contact_tag;o.x=(uint16_t)contacts[0].x;o.y=(uint16_t)contacts[0].y;
  }
  if(!count)s->contact_known=false;
  o.activity|=count!=0;
 }
 if(contacts!=local&&!s->retained)free(contacts);
 return o;
}

static inline bool home_canvas_orientation(home_canvas_surface*s,uint32_t orientation,bool reverse){
 if(!s||s->retained||s->lease||s->receipt||!risc_canvas_has_orientation(s->presentation))return false;
 if(!home_canvas_result(s,s->presentation->orientation(s->presentation->context,s->session,orientation,reverse?1u:0u)))return false;
 s->contact_known=false;return home_canvas_geometry(s);
}

static inline bool home_canvas_copy_completed(home_canvas_surface*s,void*pixels,size_t size,uint32_t stride){
 if(!s||s->retained||s->lease||s->receipt||!risc_canvas_has_snapshot(s->presentation)||!home_canvas_geometry(s))return false;
 if(!home_canvas_result(s,s->presentation->copy_completed(s->presentation->context,s->session,s->geometry.revision,pixels,size,stride)))return false;
 if(s->format==RISC_DISPLAY_FORMAT_MONO1){
  size_t row=((size_t)s->geometry.width+7)/8;
  for(uint32_t y=0;y<s->geometry.height;y++)for(size_t x=0;x<row;x++)((unsigned char*)pixels)[(size_t)y*stride+x]^=255u;
 }
 return true;
}
