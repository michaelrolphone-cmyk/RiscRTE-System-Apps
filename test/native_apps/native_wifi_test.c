/* Real controller, adapter and native primitives; synthetic capability providers. */
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
#define WIFI_WORKFLOW_LIBRARY
#include NATIVE_WIFI_WORKFLOW_FIXTURE
static bool native_retain(void){assert(!"unexpected retention");return true;}
static void ntap(unsigned at,wn_rect r){assert(r.w>0&&r.h>0);event(at,0,r.x+r.w/2,r.y+r.h/2);}
static void frame(void){render();assert(!failed);assert(paper_frame_drain());if(!portable_wifi_native_ready())fprintf(stderr,"pending=%d idle=%d dirty=%d token=%llu surface=%llu failed=%d\n",wn_pending,portable_paper_frame_idle(),dirty,(unsigned long long)paper_token,(unsigned long long)surface.frame,failed);assert(portable_wifi_native_ready());}
static void bounds(wn_rect r){assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=width()&&r.y+r.h<=height());}
static void geometry(void){
  assert(wn_fits());bounds(wn_back());for(unsigned i=0;i<3;i++)bounds(wn_footer(i));
  for(unsigned i=0;i<11;i++){
    choice=i;frame();assert(!view.keyboard);unsigned row=i%wn_rows();wn_rect r=wn_row(row);bounds(r);
    assert(portable_wifi_hit(&view,r.x+r.w/2,r.y+r.h/2)==(int)i);
  }
  for(unsigned p=0;p<PWK_PAGES;p++){
    page=WP_PASSWORD;key_page=p;strcpy(editor,"synthetic-password-with-long-tail");choice=0;frame();assert(view.keyboard);
    for(unsigned i=0;i<PWK_COUNT;i++){wn_rect r=wn_key(i);bounds(r);assert(portable_wifi_hit(&view,r.x+r.w/2,r.y+r.h/2)==(int)i);}
    assert(!strstr(view.entry,"synthetic"));
  }
  for(unsigned p=0;p<7;p++){wifi_page(p);frame();}
  assert(!writes&&!connects);
}
int main(int argc,char**argv){
  assert(argc==2);unsigned which=atoi(argv[1]);workflow_start(false);limit_polls=100000;
  if(which==0){geometry();finish();return 0;}
  if(which==1){ /* All visible footer transitions and repeat entry use physical coordinates. */
    frame();assert(portable_wifi_close());unsigned at=ticks+300;
    ntap(at,wn_footer(2));at+=400;ntap(at,wn_footer(0));at+=400;
    for(unsigned i=0;i<3;i++){ntap(at,wn_footer(2));at+=400;}
    ntap(at,wn_footer(1));at+=400;ntap(at,wn_footer(1));at+=400;ntap(at,wn_back());
    app_main();assert(scan_starts==1&&scan_cancels==1&&launches==1&&!opened&&!writes);
    script_count=0;ntap(ticks+300,wn_back());app_main();assert(launches==2&&!opened);finish();
  }else if(which==2){ /* Shared masked editor: completion only after confirmed close. */
    draft();wifi_edit(WP_PASSWORD);assert(wifi_text_active&&text_live&&text_request.reserved&RISC_TEXT_ENTRY_REQUEST_MASKED);
    text_close_wait=2;text_done(RISC_TEXT_ENTRY_ACCEPTED,"new-synthetic-password",0);assert(wifi_text_active&&!strcmp(credentials.password,"testpass123"));
    wifi_text_step();assert(wifi_text_active);wifi_text_step();assert(!wifi_text_active&&!strcmp(credentials.password,"new-synthetic-password")&&!writes);finish();
  }else if(which==3){ /* Display completes late: a held old-page contact cannot activate new rows. */
    native_present_wait=600;frame();assert(portable_wifi_close());
    unsigned at=ticks+100;ntap(at,wn_footer(1));ntap(at+1200,wn_back());app_main();
    assert(!writes&&!connects&&!text_opens&&launches==1&&!opened);finish();
  }else if(which==4){ /* Page mutation invalidates a held gesture before release. */
    frame();wn_rect r=wn_row(0);unsigned at=ticks+50;ntap(at,r);
    t5_app_input_t in;while(ticks<at+50)assert(wa->poll(&in,25));
    wifi_page(WP_HELP);frame();while(ticks<at+200){assert(wa->poll(&in,25));assert(!in.tapped);}
    assert(page==WP_HELP&&!text_opens&&!writes);finish();
  }else if(which==5){ /* Back remains responsive during a queued native frame. */
    frame();assert(portable_wifi_close());native_present_wait=800;
    event(ticks+75,T5_APP_BUTTON_BACK,-1,0);event(ticks+900,T5_APP_BUTTON_BACK,-1,0);app_main();
    assert(!opened&&launches==1&&!writes);finish();
  }else if(which==6){ /* Gaps outside visible buttons never select a row. */
    frame();assert(portable_wifi_close());wn_rect r=wn_footer(0);event(ticks+300,0,r.x+r.w+1,r.y+r.h/2);ntap(ticks+800,wn_back());app_main();assert(!text_opens&&!connects&&!writes&&launches==1);finish();
  }else assert(!"unknown native case");
  printf("Native Wi-Fi case %u %dx%d PASS\n",which,width(),height());return 0;
}
