/* Executable specification model, NOT a production renderer/provider. */
#include "proposed/RiscCanvasV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned live,quiescent,retained,blocked; uint32_t revision,lease_revision,state; uint64_t lease,receipt,next; unsigned char working[16],snapshot[16]; } model;
static void init(model*m){memset(m,0,sizeof(*m));m->live=1;m->revision=1;m->next=1;}
static int begin(model*m,uint64_t*l){if(m->retained||(m->receipt&&m->state==RISC_CANVAS_CUSTODY_RETAINED))return RISC_CANVAS_RETAINED;if(!m->live)return RISC_CANVAS_STALE;if(m->quiescent||m->blocked||m->lease)return RISC_CANVAS_BUSY;m->lease=m->next++;m->lease_revision=m->revision;*l=m->lease;return 0;}
static int submit(model*m,uint64_t l,uint64_t*r){if(m->retained||(m->receipt&&m->state==RISC_CANVAS_CUSTODY_RETAINED))return RISC_CANVAS_RETAINED;if(!m->live||l!=m->lease||!l)return RISC_CANVAS_STALE;if(m->lease_revision!=m->revision)return RISC_CANVAS_STALE;if(m->blocked||m->receipt)return RISC_CANVAS_AGAIN;memcpy(m->snapshot,m->working,16);m->lease=0;m->receipt=m->next++;m->state=RISC_CANVAS_QUEUED;*r=m->receipt;return 0;}
static int cancel(model*m,uint64_t l){if(m->retained||(m->receipt&&m->state==RISC_CANVAS_CUSTODY_RETAINED))return RISC_CANVAS_RETAINED;if(!l||l!=m->lease)return RISC_CANVAS_STALE;m->lease=0;return 0;}
static int forget(model*m,uint64_t r){if(m->retained||(m->receipt&&m->state==RISC_CANVAS_CUSTODY_RETAINED))return RISC_CANVAS_RETAINED;if(!r||r!=m->receipt)return RISC_CANVAS_STALE;if(m->state==RISC_CANVAS_CUSTODY_RETAINED)return RISC_CANVAS_RETAINED;if(m->state!=RISC_CANVAS_COMPLETE&&m->state!=RISC_CANVAS_SUPERSEDED&&m->state!=RISC_CANVAS_FAILED)return RISC_CANVAS_BUSY;m->receipt=0;m->state=0;return 0;}
static int quiesce(model*m){if(m->retained||(m->receipt&&m->state==RISC_CANVAS_CUSTODY_RETAINED))return RISC_CANVAS_RETAINED;if(m->receipt&&m->state==RISC_CANVAS_CUSTODY_RETAINED)return RISC_CANVAS_RETAINED;if(m->lease||(m->receipt&&m->state!=RISC_CANVAS_COMPLETE&&m->state!=RISC_CANVAS_SUPERSEDED&&m->state!=RISC_CANVAS_FAILED))return RISC_CANVAS_AGAIN;m->quiescent=1;return 0;}
static int close_session(model*m){int r=quiesce(m);if(r)return r;m->live=0;m->receipt=0;return 0;}
static int snapshot(const risc_canvas_event_v1 *in,size_t n,uint64_t cut,risc_canvas_event_v1 *out,size_t cap,size_t *needed,uint64_t *sequence){
 *needed=n;if(!out&&cap==0)return 0;if(cap<n)return RISC_CANVAS_AGAIN;memcpy(out,in,n*sizeof(*out));*sequence=cut;return 0;
}
int main(void){model m;uint64_t l=0,r=0,out=999;unsigned cases=0;
 init(&m);assert(begin(&m,&l)==0&&l);cases++;
 m.blocked=1;assert(submit(&m,l,&out)==RISC_CANVAS_AGAIN&&m.lease==l&&out==999);cases++;
 m.blocked=0;m.revision=2;assert(submit(&m,l,&out)==RISC_CANVAS_STALE&&m.lease==l&&out==999);cases++;
 assert(cancel(&m,l)==0);assert(begin(&m,&l)==0);memset(m.working,0x57,16);assert(submit(&m,l,&r)==0&&!m.lease&&r);cases++;
 assert(cancel(&m,l)==RISC_CANVAS_STALE&&m.receipt==r);cases++;
 assert(forget(&m,r)==RISC_CANVAS_BUSY&&m.receipt==r);cases++;
 assert(quiesce(&m)==RISC_CANVAS_AGAIN&&!m.quiescent);cases++;
 /* A new writable frame must never alias queued snapshot backing. */
 assert(begin(&m,&l)==0);memset(m.working,0x91,16);for(unsigned i=0;i<16;i++)assert(m.snapshot[i]==0x57);cases++;
 assert(cancel(&m,l)==0&&!m.lease);cases++;
 m.state=RISC_CANVAS_SUPERSEDED;assert(quiesce(&m)==0);assert(begin(&m,&l)==RISC_CANVAS_BUSY);cases++;
 assert(forget(&m,r)==0);assert(forget(&m,r)==RISC_CANVAS_STALE);cases++;
 m.quiescent=0;assert(begin(&m,&l)==0);assert(close_session(&m)==RISC_CANVAS_AGAIN&&m.live);assert(cancel(&m,l)==0);assert(close_session(&m)==0&&!m.live);cases++;
 init(&m);assert(begin(&m,&l)==0);m.retained=1;assert(cancel(&m,l)==RISC_CANVAS_RETAINED&&m.lease==l);assert(close_session(&m)==RISC_CANVAS_RETAINED&&m.live);cases++;
 init(&m);assert(begin(&m,&l)==0);assert(submit(&m,l,&r)==0);m.state=RISC_CANVAS_CUSTODY_RETAINED;assert(!m.retained);assert(begin(&m,&out)==RISC_CANVAS_RETAINED);assert(forget(&m,r)==RISC_CANVAS_RETAINED&&m.receipt==r);assert(quiesce(&m)==RISC_CANVAS_RETAINED&&!m.quiescent);assert(close_session(&m)==RISC_CANVAS_RETAINED&&m.live);cases++;
 assert(!risc_canvas_get_v1(NULL,0));risc_canvas_api_v1 a={0};assert(!risc_canvas_get_v1(&a,sizeof a));a.api_version=1;a.struct_size=sizeof a;a.tag=RISC_CANVAS_TAG;assert(!risc_canvas_get_v1(&a,sizeof a));assert(!risc_canvas_get_v1(&a,sizeof a-1));cases++;
 risc_canvas_event_v1 events[2]={{0}},copied[2]={{0}};size_t needed=0;uint64_t cut=99;
 events[0].contact=7;events[1].contact=8;events[0].geometry_revision=events[1].geometry_revision=3;
 assert(snapshot(events,1,11,NULL,0,&needed,&cut)==0&&needed==1&&cut==99);cases++;
 copied[0].contact=123;assert(snapshot(events,2,12,copied,1,&needed,&cut)==RISC_CANVAS_AGAIN&&needed==2&&cut==99&&copied[0].contact==123);cases++;
 assert(snapshot(events,2,12,copied,2,&needed,&cut)==0&&cut==12&&copied[0].contact==7&&copied[1].contact==8&&copied[0].geometry_revision==3);cases++;
 /* An older geometry event cannot update a current contact; only sequence
  * greater than snapshot cut and equal geometry applies after reconciliation. */
 risc_canvas_event_v1 motion={0};motion.sequence=13;motion.geometry_revision=2;
 assert(!(motion.sequence>cut&&motion.geometry_revision==copied[0].geometry_revision));motion.geometry_revision=3;assert(motion.sequence>cut&&motion.geometry_revision==copied[0].geometry_revision);cases++;
 printf("%u canvas ownership specification cases passed; model only\n",cases);return 0;}
