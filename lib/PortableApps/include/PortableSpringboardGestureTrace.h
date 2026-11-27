#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Explicit app-local DIAGNOSTIC schema, not generic performance metrics or a
 * movement stream. Only a selected Springboard emits a terminal gesture summary.
 * Tags E0..E4 are schema-owned COUNTER payloads, never pointer/string data. */
#define PORTABLE_SB_DIAG_SCHEMA 1u
#define PORTABLE_SB_DIAG_RECORD_BOUND 14u
#define PORTABLE_SB_DIAG_META 0xe0u
#define PORTABLE_SB_DIAG_CONTACT 0xe1u
#define PORTABLE_SB_DIAG_PROVIDER 0xe2u
#define PORTABLE_SB_DIAG_MAPPED 0xe3u
#define PORTABLE_SB_DIAG_DELTA 0xe4u
typedef struct {uint64_t sequence;uint16_t px,py,x,y,width,height;uint8_t id;bool valid,began,released;} portable_sb_diag_sample;
typedef struct {portable_sb_diag_sample sample;int16_t dx,dy;uint8_t kind;} portable_sb_diag_point;
typedef struct {portable_sb_diag_point points[4];uint64_t sequence;uint8_t mask,id;int8_t sx,sy;bool active;} portable_sb_diag;
typedef void (*portable_sb_diag_emit_fn)(void*,uint32_t);
static inline void portable_sb_diag_clear(portable_sb_diag*s){memset(s,0,sizeof(*s));}
static inline bool portable_sb_diag_valid(const portable_sb_diag_sample*p){return p->valid&&p->sequence&&p->width&&p->height&&p->width<=1024&&p->height<=1024&&p->px<p->width&&p->py<p->height&&p->x<p->width&&p->y<p->height&&p->sequence!=UINT64_MAX;}
static inline uint32_t portable_sb_diag_pair(unsigned tag,unsigned slot,unsigned kind,int x,int y){return (tag<<24)|((slot&3u)<<22)|((kind&3u)<<20)|(((uint32_t)x&1023u)<<10)|((uint32_t)y&1023u);}
/* kind0 begins,1 is an actual dispatched held delta,2 ends. Invalid/cancelled
 * ownership clears without transport. Zero motion and duplicate reads are not
 * samples. A sign reversal is selected once, not every move/poll. */
static inline void portable_sb_diag_step(portable_sb_diag*s,const portable_sb_diag_sample*p,unsigned kind,int dx,int dy,unsigned rotation,portable_sb_diag_emit_fn emit,void*context){
 if(!portable_sb_diag_valid(p)||kind>2||dx< -256||dx>256||dy< -256||dy>256||(rotation!=0&&rotation!=180)){portable_sb_diag_clear(s);return;}
 if(kind==0){portable_sb_diag_clear(s);if(!p->began||p->released)return;s->active=true;s->id=p->id;s->sequence=p->sequence;s->mask=1;s->points[0]=(portable_sb_diag_point){*p,0,0,0};return;}
 if(!s->active)return;
 if(p->id!=s->id||p->sequence<s->sequence||p->width!=s->points[0].sample.width||p->height!=s->points[0].sample.height){portable_sb_diag_clear(s);return;}
 if(p->sequence==s->sequence)return;
 s->sequence=p->sequence;
 if(kind==1){
  if(p->released){portable_sb_diag_clear(s);return;}if(!dx&&!dy)return;
  int sx=(dx>0)-(dx<0),sy=(dy>0)-(dy<0);portable_sb_diag_point point={*p,(int16_t)dx,(int16_t)dy,1};
  if(!(s->mask&2)){s->points[1]=point;s->mask|=2;}
  else if(!(s->mask&4)&&((sx&&s->sx&&sx!=s->sx)||(sy&&s->sy&&sy!=s->sy))){s->points[2]=point;s->mask|=4;}
  if(sx)s->sx=(int8_t)sx;
  if(sy)s->sy=(int8_t)sy;
  return;
 }
 if(!p->released){portable_sb_diag_clear(s);return;}
 s->points[3]=(portable_sb_diag_point){*p,(int16_t)dx,(int16_t)dy,2};s->mask|=8;
 if(emit){
  emit(context,(PORTABLE_SB_DIAG_META<<24)|(PORTABLE_SB_DIAG_SCHEMA<<21)|((rotation==180?1u:0u)<<20)|(((uint32_t)p->width-1)<<10)|((uint32_t)p->height-1));
  emit(context,(PORTABLE_SB_DIAG_CONTACT<<24)|((uint32_t)s->mask<<8)|s->id);
  for(unsigned i=0;i<4;i++)if(s->mask&(1u<<i)){const portable_sb_diag_point*q=&s->points[i];emit(context,portable_sb_diag_pair(PORTABLE_SB_DIAG_PROVIDER,i,q->kind,q->sample.px,q->sample.py));emit(context,portable_sb_diag_pair(PORTABLE_SB_DIAG_MAPPED,i,q->kind,q->sample.x,q->sample.y));emit(context,portable_sb_diag_pair(PORTABLE_SB_DIAG_DELTA,i,q->kind,q->dx,q->dy));}
 }
 portable_sb_diag_clear(s);
}
#ifdef PORTABLE_SPRINGBOARD_GESTURE_TRACE
void portable_springboard_gesture_dispatch(unsigned kind,int x,int y,int dx,int dy);
void portable_springboard_gesture_cancel(void);
#endif
