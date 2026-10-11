/* Real adapter: an opted-in app owns tabs and plots over the whole surface. */
#include "nova_peripherals.h"
#include "PortableNovaUi.h"
void app_main(void) {
    const t5_app_api_v1 *api=t5_app_get_api(1);
    actions[0].at=3; actions[0].x=30; actions[0].y=25;
    actions[1].at=6; actions[1].x=60; actions[1].y=120;
    actions[2].at=7; actions[2].x=180; actions[2].y=120;
    action_count=3;
    assert(api->set_back_exits_app); api->set_back_exits_app(false);
    bool seen=false,drag_start=false,drag_move=false,released=false;
    assert(api->touch_contact);
    for(unsigned i=0;i<12;i++) {
        t5_app_input_t in={0}; assert(api->poll(&in,25));
        t5_app_contact_t contact={0};assert(api->touch_contact(&contact));
        if(contact.down&&contact.x==60&&contact.y==120)drag_start=true;
        if(contact.down&&contact.x==180&&contact.y==120)drag_move=true;
        if(drag_move&&!contact.down)released=true;
        assert(!in.exit_requested && !(in.buttons&T5_APP_BUTTON_BACK));
        if(in.tapped){assert(in.touch_x==30&&in.touch_y==25);seen=true;}
    }
    assert(seen&&drag_start&&drag_move&&released); api->set_back_exits_app(true);
}
