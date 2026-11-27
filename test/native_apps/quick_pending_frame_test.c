/* Owning state-machine tests. Service callbacks are deliberate doubles. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "PortableQuickPendingFrame.h"

typedef struct {
    bool safe, paint_ok, pending_busy, cancel_on_ready;
    int cancel, ready, calls, paints, reads, cancels, safety;
    int lose_on; /* callback kind: 1 cancellation, 2 readiness, 3 paint */
} model;
static void allowed(model *m) { assert(m->safe); m->calls++; }
static bool safe(void *p) { model *m=p; m->safety++; return m->safe; }
static int cancelled(void *p) { model *m=p; allowed(m); m->cancels++; if(m->lose_on==1)m->safe=false; return m->cancel; }
static int ready(void *p) { model *m=p; allowed(m); m->reads++; if(m->cancel_on_ready)m->cancel=1; if(m->lose_on==2)m->safe=false; return m->ready; }
static bool paint(void *p) { model *m=p; allowed(m); m->paints++; if(m->pending_busy)m->ready=0; if(m->lose_on==3)m->safe=false; return m->paint_ok; }
static const struct portable_quick_pending_ops ops={safe,cancelled,ready,paint};
static model fresh(void) { model m; memset(&m,0,sizeof(m));m.safe=true;m.paint_ok=true;return m; }
static int step(portable_quick_pending_frame *s, model *m) {return portable_quick_pending_step(s,&ops,m);}
static void terminal(portable_quick_pending_frame *s,model *m,int expected) {
    int calls=m->calls,safety=m->safety;
    for(int i=0;i<8;i++)assert(step(s,m)==expected);
    assert(m->calls==calls && m->safety==safety);
}
/* Drive one callback at a time if implementation yields at phase boundaries. */
static int advance(portable_quick_pending_frame *s,model *m) {
    for(int i=0;i<8;i++) {int r=step(s,m);if(r!=PENDING_WAIT)return r;}
    return PENDING_WAIT;
}
int main(void) {
    unsigned cases=0;
    {
        portable_quick_pending_frame s={0};model m=fresh();m.ready=1;
        assert(advance(&s,&m)==PENDING_READY);assert(m.paints==1);
        terminal(&s,&m,PENDING_READY);cases++;
    }
    {
        portable_quick_pending_frame s={0};model m=fresh();
        for(int i=0;i<12;i++)assert(step(&s,&m)==PENDING_WAIT);
        assert(m.paints==0); /* prior frame has not completed */
        m.ready=1;m.pending_busy=true;
        /* The first matching-frame readiness callback must not use the
           predecessor readiness result. Switch to busy inside paint below. */
        assert(advance(&s,&m)==PENDING_WAIT);assert(m.paints==1);
        for(int i=0;i<12;i++)assert(step(&s,&m)==PENDING_WAIT);
        assert(m.paints==1);m.ready=1;assert(advance(&s,&m)==PENDING_READY);
        terminal(&s,&m,PENDING_READY);cases++;
    }
    /* A submitted pending frame, distinct from the predecessor, remains busy.
       Initial phase2 is an intentional state-machine seam, not a fake present. */
    {
        portable_quick_pending_frame s={2};model m=fresh();
        for(int i=0;i<12;i++)assert(step(&s,&m)==PENDING_WAIT);
        assert(m.paints==0);m.ready=1;assert(advance(&s,&m)==PENDING_READY);
        terminal(&s,&m,PENDING_READY);cases++;
    }
    for(unsigned phase=0;phase<=2;phase++) {
        /* Back, Cancel, malformed contact all use the same cancellation
           authority; none may apply a preference or repaint after cancel. */
        portable_quick_pending_frame s={phase};model m=fresh();m.cancel=1;m.ready=1;
        assert(step(&s,&m)==PENDING_CANCELLED);assert(m.paints==0 && m.reads==0);
        terminal(&s,&m,PENDING_CANCELLED);cases++;
    }
    for(unsigned phase=0;phase<=2;phase++) {
        portable_quick_pending_frame s={phase};model m=fresh();m.safe=false;
        assert(step(&s,&m)==PENDING_FAULT);assert(m.calls==0);
        terminal(&s,&m,PENDING_FAULT);cases++;
    }
    for(unsigned phase=0;phase<=2;phase++) {
        portable_quick_pending_frame s={phase};model m=fresh();m.cancel=-1;
        assert(step(&s,&m)==PENDING_FAULT);assert(m.paints==0 && m.reads==0);
        terminal(&s,&m,PENDING_FAULT);cases++;
    }
    for(int callback=1;callback<=3;callback++) {
        portable_quick_pending_frame s={0};model m=fresh();m.ready=1;m.lose_on=callback;
        assert(advance(&s,&m)==PENDING_FAULT);assert(!m.safe);
        terminal(&s,&m,PENDING_FAULT);cases++;
    }
    for(unsigned phase=0;phase<=2;phase+=2) {
        portable_quick_pending_frame s={phase};model m=fresh();m.ready=-1;
        assert(step(&s,&m)==PENDING_FAULT);assert(m.paints==0);
        terminal(&s,&m,PENDING_FAULT);cases++;
    }
    {
        portable_quick_pending_frame s={1};model m=fresh();m.ready=1;m.paint_ok=false;
        assert(step(&s,&m)==PENDING_FAULT);assert(m.paints==1);
        terminal(&s,&m,PENDING_FAULT);cases++;
    }
    {
        portable_quick_pending_frame s={2};model m=fresh();m.ready=1;m.cancel_on_ready=true;
        assert(step(&s,&m)==PENDING_CANCELLED);assert(m.cancels==2);
        terminal(&s,&m,PENDING_CANCELLED);cases++;
    }
    {
        portable_quick_pending_frame s={99};model m=fresh();
        assert(step(&s,&m)==PENDING_FAULT);assert(m.calls==0 && m.safety==0);
        terminal(&s,&m,PENDING_FAULT);cases++;
    }
    {
        portable_quick_pending_frame s={0};model m=fresh();
        assert(portable_quick_pending_step(NULL,&ops,&m)==PENDING_FAULT);
        assert(portable_quick_pending_step(&s,NULL,&m)==PENDING_FAULT);
        for(unsigned i=0;i<4;i++) {
            struct portable_quick_pending_ops missing=ops;
            if(i==0)missing.safe=NULL;
            if(i==1)missing.cancelled=NULL;
            if(i==2)missing.ready=NULL;
            if(i==3)missing.paint=NULL;
            assert(portable_quick_pending_step(&s,&missing,&m)==PENDING_FAULT);
        }
        assert(m.calls==0 && m.safety==0);cases++;
    }
    printf("PASS Quick pending-frame state machine %u cases; injected services, not Runtime/device\n",cases);
    return 0;
}
