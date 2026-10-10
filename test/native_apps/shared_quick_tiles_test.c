#include "PortableQuickActions.h"
#include <assert.h>
#include <stdio.h>

static pqa_state sheet(bool audio) {
    pqa_state s;
    pqa_init(&s);
    s.paper=true;s.audio_controls=audio;s.clean_refresh_valid=true;
    s.brightness_valid=true;s.volume_valid=true;s.brightness=40;
    s.position_q8=s.target_q8=PQA_OPEN_Q8;
    return s;
}
static uint32_t tap(pqa_state *s,int x,int y) {
    assert(pqa_input(s,10,true,1,1,x,y,true));
    assert(pqa_input(s,20,true,0,0,0,0,true));
    return pqa_take_action(s);
}
int main(void) {
    for(unsigned audio=0;audio<2;audio++) {
        pqa_state s=sheet(audio!=0);
        int xs[9],ys[9];bool shown[9];unsigned count=0;
        for(unsigned tile=0;tile<9;tile++) {
            shown[tile]=pqa_paper_tile(&s,tile,&xs[tile],&ys[tile]);
            if(!shown[tile])continue;
            assert(xs[tile]==32||xs[tile]==244);
            assert(ys[tile]>180&&ys[tile]+92<714);
            for(unsigned other=0;other<tile;other++)
                assert(!shown[other]||xs[tile]!=xs[other]||ys[tile]!=ys[other]);
            ++count;
        }
        assert(!shown[6]&&shown[0]==(audio!=0));
#ifdef PORTABLE_QUICK_USB_TRANSFER
        assert(shown[7]&&count==(audio?8u:7u));
        assert(xs[7]==(audio?32:244)&&ys[7]==(audio?614:414));
        for(unsigned repeat=0;repeat<100;repeat++) {
            s=sheet(audio!=0);
            assert(tap(&s,xs[7]+102,ys[7]+46)==PQA_USB_TRANSFER);
            assert(!pqa_take_action(&s)&&s.target_q8==0);
            s=sheet(audio!=0);
            assert(pqa_input(&s,10,true,1,1,xs[7]+102,ys[7]+46,true));
            assert(pqa_input(&s,20,true,1,1,xs[7]+126,ys[7]+46,true));
            assert(pqa_input(&s,30,true,0,0,0,0,true));
            assert(!pqa_take_action(&s)&&s.target_q8==PQA_OPEN_Q8);
            s=sheet(audio!=0);
            assert(pqa_input(&s,10,true,1,1,xs[7]+102,ys[7]+46,true));
            assert(pqa_input(&s,20,false,0,0,0,0,true));
            assert(!pqa_take_action(&s)&&s.neutral_gate);
            assert(pqa_input(&s,30,true,0,0,0,0,true));
            assert(tap(&s,xs[7]+102,ys[7]+46)==PQA_USB_TRANSFER);
        }
#else
        assert(!shown[7]&&count==(audio?7u:6u));
#endif
        s=sheet(audio!=0);
        assert(tap(&s,xs[8]+102,ys[8]+46)==PQA_CLEAN_REFRESH);
        s=sheet(audio!=0);s.clean_refresh_valid=false;
        int x,y;assert(!pqa_paper_tile(&s,8,&x,&y));
        if(!audio) {s=sheet(false);assert(!tap(&s,440,550));}
    }
    puts("Shared host tiles: capability layout, boundaries, 100 repeated/cancelled USB interactions PASS");
}
