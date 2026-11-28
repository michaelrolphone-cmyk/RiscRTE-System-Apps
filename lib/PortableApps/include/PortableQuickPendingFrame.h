#ifndef PORTABLE_QUICK_PENDING_FRAME_H
#define PORTABLE_QUICK_PENDING_FRAME_H
#include <stdbool.h>
enum { PENDING_FAULT=-1,PENDING_WAIT=0,PENDING_READY=1,PENDING_CANCELLED=2 };
typedef struct {unsigned phase;} portable_quick_pending_frame;
typedef struct portable_quick_pending_ops {
 bool (*safe)(void*);
 int (*cancelled)(void*); /* -1 fault, 0 continue, 1 cancel */
 int (*ready)(void*);     /* progress existing submission: -1 fault, 0 busy, 1 idle */
 bool (*paint)(void*);   /* create and submit this exact pending image once */
} portable_quick_pending_ops;
/* Phase2 can only follow this barrier's paint; an older idle observation can
 * never qualify its completion. Terminal results perform no further callbacks. */
static inline int portable_quick_pending_step(portable_quick_pending_frame*s,const portable_quick_pending_ops*o,void*c){
 if(!s||!o||!o->safe||!o->cancelled||!o->ready||!o->paint)return PENDING_FAULT;
 if(s->phase==3)return PENDING_READY;
 if(s->phase==4)return PENDING_CANCELLED;
 if(s->phase>=5)return PENDING_FAULT;
 if(!o->safe(c)){s->phase=5;return PENDING_FAULT;}
 int cancel=o->cancelled(c);
 if(cancel<0||!o->safe(c)){s->phase=5;return PENDING_FAULT;}
 if(cancel){s->phase=4;return PENDING_CANCELLED;}
 if(s->phase==1){
  bool painted=o->paint(c);
  if(!painted||!o->safe(c)){s->phase=5;return PENDING_FAULT;}
  s->phase=2;return PENDING_WAIT;
 }
 int ready=o->ready(c);
 if(ready<0||!o->safe(c)){s->phase=5;return PENDING_FAULT;}
 if(!ready)return PENDING_WAIT;
 if(s->phase==0){s->phase=1;return PENDING_WAIT;}
 /* Progress may service input. Check cancellation at this settled boundary,
  * rather than submit after a contact/Back observed during completion. */
 cancel=o->cancelled(c);
 if(cancel<0||!o->safe(c)){s->phase=5;return PENDING_FAULT;}
 if(cancel){s->phase=4;return PENDING_CANCELLED;}
 s->phase=3;return PENDING_READY;
}
#endif
