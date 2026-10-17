/* Local keyboard profile remains explicit; the full delivered recipe uses the
 * copied shared editor. Exercise both instead of inventing a fallback. */
#define PORTABLE_WIFI_NATIVE_UI
#define PORTABLE_NATIVE_CANVAS
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_NOVA_UI
#define PORTABLE_ASYNC_RGB565
#define PORTABLE_FORCE_FULL_FRAMES
#define PORTABLE_TOUCH_ROTATION 0
#include <stdbool.h>
static bool native_retain(void);
#define TEST_WIFI_RUNTIME_BINDINGS .retain_invocation=native_retain,
#define main retained_fixture_main
#include NATIVE_WIFI_LOCAL_FIXTURE
#undef main
static bool native_retain(void){assert(!"unexpected retention");return true;}
static void kframe(void){render();assert(portable_paper_frame_drain());assert(!failed&&portable_wifi_native_ready());}
static void ktap(unsigned at,wn_rect r){event(at,0,r.x+r.w/2,r.y+r.h/2);}
int main(int argc,char**argv){
  assert(argc==2);unsigned mode=atoi(argv[1]);start();draft();
  if(mode==0){
    wifi_edit(WP_PASSWORD);assert(page==WP_PASSWORD&&key_page==PWK_INITIAL_PAGE);kframe();
    assert(!strstr(view.entry,"testpass"));wifi_key(PWK_DELETE);wifi_key(1);wifi_key(PWK_DONE);assert(page==WP_ROOT&&!strcmp(credentials.password,"testpass12a")&&!writes);
    for(unsigned p=0;p<PWK_PAGES;p++)for(unsigned i=0;i<PWK_CHARACTERS;i++){
      wn_rect r=wn_key(i);unsigned ch=portable_watch_key_character(p,i);if(!ch)continue;
      unsigned face=r.h<18?3:2;const rps_glyph*g=&rps_text[face*95+ch-32];assert((int)g->height<=r.h);assert((unsigned)(g->advance_q4+15)/16u<=(unsigned)r.w-2u);
    }
    wifi_edit(WP_SSID);strcpy(editor,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef");kframe();assert(!strcmp(view.entry,editor));
  }else if(mode==1){
    kframe();assert(portable_wifi_close());unsigned at=ticks+300;
    ktap(at,wn_footer(2));at+=400;ktap(at,wn_footer(1));at+=400;
    ktap(at,wn_key(1));at+=400;ktap(at,wn_key(PWK_DELETE));at+=400;ktap(at,wn_key(PWK_PAGE));at+=400;ktap(at,wn_key(PWK_DONE));at+=400;
    ktap(at,wn_back());app_main();assert(!opened&&launches==1&&!writes&&!connects);
  }else if(mode==2){
    wifi_edit(WP_PASSWORD);for(unsigned p=0;p<PWK_PAGES;p++){key_page=p;kframe();}
  }else assert(!"unknown keyboard mode");
  finish();printf("Native Wi-Fi keyboard %u %dx%d PASS\n",mode,width(),height());return 0;
}
