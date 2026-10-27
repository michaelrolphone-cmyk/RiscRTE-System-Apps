#define main inherited_wifi_main
#include WIFI_NATIVE_FIXTURE
#undef main
#include "PortableWifiNativeLayout.h"
static jmp_buf done;
static unsigned stop_at;
static void observed_yield(uint32_t ms){fake_yield(ms);if(poll_count>=stop_at)longjmp(done,1);}
static void tap(unsigned *at,wifi_native_rect r){held_touch(*at,r.x+r.w/2,r.y+r.h/2);*at+=30;}
int main(void){
 start();assert(portable_wifi_close());runtime_api.yield_ms=observed_yield;limit_polls=2000;
 wifi_native_layout l;assert(wifi_native_layout_open(&l,width(),height()));unsigned at=20;
 tap(&at,l.rows[0]); /* SSID. Initial lowercase then symbols then uppercase. */
 tap(&at,l.keys[1]);tap(&at,l.keys[PWK_PAGE]);tap(&at,l.keys[1]);tap(&at,l.keys[PWK_PAGE]);tap(&at,l.keys[1]);tap(&at,l.keys[PWK_DONE]);
 tap(&at,l.rows[1]);for(unsigned i=0;i<8;i++)tap(&at,l.keys[1]);tap(&at,l.keys[PWK_DONE]);
 /* Prev wraps to9, Next returns0, Select edits SSID. Back cancels only editor. */
 tap(&at,l.footer[0]);tap(&at,l.footer[2]);tap(&at,l.footer[1]);tap(&at,l.keys[2]);tap(&at,l.back);
 stop_at=at+10;if(!setjmp(done))app_main();
 assert(opened&&page==WP_ROOT&&choice==0&&!strcmp(credentials.ssid,"a!A")&&!strcmp(credentials.password,"aaaaaaaa")&&draft_dirty&&!writes&&!connects);
 assert(!editor[0]);wifi_edit(WP_PASSWORD);wifi_make_view();assert(!strcmp(view.entry,"********"));
 unsigned before=writes;assert(portable_wifi_suspend());portable_wifi_resume();assert(!strcmp(editor,"aaaaaaaa")&&!strcmp(credentials.ssid,"a!A")&&!strcmp(credentials.password,"aaaaaaaa")&&writes==before);
 wifi_key(PWK_DELETE);wifi_make_view();assert(!strcmp(view.entry,"*******"));assert(!wifi_back());assert(!strcmp(credentials.password,"aaaaaaaa")&&!writes);
 /* Explicit Save remains the only write. */wifi_activate(6);assert(writes>0);finish();
 printf("Actual native controller+adapter %dx%d: tap all ASCII pages, draft Done vs Save, footer, Back, password mask/delete and suspend/resume PASS\n",WIFI_TEST_W,WIFI_TEST_H);
}
