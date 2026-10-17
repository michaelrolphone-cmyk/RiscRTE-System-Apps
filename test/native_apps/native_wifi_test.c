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
static bool native_exit(void);
static bool expect_retain;static unsigned native_retains;
#define TEST_WIFI_RUNTIME_BINDINGS .retain_invocation=native_retain,.exit_requested=native_exit,
#define WIFI_WORKFLOW_LIBRARY
#include NATIVE_WIFI_WORKFLOW_FIXTURE
static bool native_retain(void){assert(expect_retain);++native_retains;return true;}
static unsigned native_exit_at;
static bool native_exit(void){return native_exit_at&&ticks>=native_exit_at;}
static void ntap(unsigned at,wn_rect r){assert(r.w>0&&r.h>0);event(at,0,r.x+r.w/2,r.y+r.h/2);}
static void frame(void){render();assert(!failed);assert(paper_frame_drain());assert(portable_wifi_native_ready());}
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
  assert(argc==2);unsigned which=atoi(argv[1]);native_sync_display=which==7;native_zero_timestamps=which==8;if(which==9)native_clock_base=UINT32_MAX-50u;workflow_start(which==15);limit_polls=100000;
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
  }else if(which>=7&&which<=10){
    /* Existing reset watermark gates captured input independently of clocks.
     * Both synchronous presentation and explicit async drain service raw input. */
    frame();
    if(which==10){
      /* A completion owned by an overlay must not erase its fresh contact. */
      wn_rect r=wn_row(0);unsigned at=ticks+50;ntap(at,r);t5_app_input_t in;
      while(ticks<at+50)assert(wa->poll(&in,10));
      assert(wn_gesture&&!native_present_token);portable_native_presented(UINT64_MAX);assert(wn_gesture);
      unsigned taps=0;while(ticks<at+180){assert(wa->poll(&in,10));taps+=in.tapped;}assert(taps==1);
    }else{
      native_present_wait=600;ntap(ticks+100,wn_footer(1));frame();
      unsigned until=ticks+150,taps=0;t5_app_input_t in;
      while(ticks<until){assert(wa->poll(&in,10));taps+=in.tapped;}assert(!taps&&!wn_gesture);
      unsigned at=ticks+50;ntap(at,wn_footer(2));while(ticks<at+200){assert(wa->poll(&in,10));taps+=in.tapped;}assert(taps==1);
      if(which==9)assert((uint32_t)(native_clock_base+ticks)<2000);
    }
    finish();
  }else if(which==11){
    for(unsigned p=0;p<7;p++){wifi_page(p);frame();}finish();
  }else if(which==12){
    frame();native_snapshot_fail=true;expect_retain=true;render();(void)paper_frame_drain();
    assert(native_retains==1&&native_custody_retained&&failed&&!portable_wifi_native_ready());
    unsigned count=presents;portable_wifi_native_presented();t5_app_input_t in;assert(!wa->poll(&in,0));assert(presents==count&&native_retains==1);
    puts("Native presentation reset snapshot failure retained PASS");return 0;
  }else if(which==13){
    frame();native_present_wait=600;
    assert(portable_paper_frame_ready());clear();fill(0,0,width(),height(),0x1234);present(false);
#ifdef PORTABLE_RASTER_SNAPSHOT
    for(unsigned i=0;i<1000&&!paper_token;i++)assert(paper_present_progress());
#endif
    assert(paper_token&&native_token_owner==0);risc_display_present_token_v1 overlay=paper_token;
#ifdef PORTABLE_RASTER_SNAPSHOT
    wifi_page(WP_HELP);render();assert(raster_sealed&&raster_native_present_owner==2);
#endif
    wn_gesture=true;unsigned snapshots=native_snapshots;
    assert(paper_token_progress());ticks+=600;assert(paper_token_progress());
    assert(!paper_token&&wn_gesture&&native_snapshots==snapshots);
    portable_native_presented(overlay);assert(wn_gesture&&native_snapshots==snapshots);
#ifdef PORTABLE_RASTER_SNAPSHOT
    assert(!portable_wifi_native_ready());assert(paper_frame_drain());
    assert(!wn_gesture&&native_snapshots>snapshots&&portable_wifi_native_ready());
#else
    frame();assert(!wn_gesture&&native_snapshots>snapshots);
#endif
    finish();
  }else if(which==14){
    frame();assert(portable_wifi_close());native_present_wait=600;native_exit_at=ticks+100;app_main();
    assert(!opened&&!writes&&!native_retains&&ticks<3000);native_exit_at=0;finish();
  }else if(which==15){
    wifi_scan_start();frame();unsigned at=ticks+50;ntap(at,wn_footer(1));t5_app_input_t in;
    while(ticks<at+50){assert(wa->poll(&in,10));}assert(wn_gesture);
    progress.phase=RISC_RADIO_RESULTS;progress.scan.state=GARDEN_RADIO_SCAN_DONE;progress.scan.count=1;
    strcpy(progress.scan.entries[0].ssid,"Fresh AP");progress.scan.entries[0].auth=GARDEN_RADIO_AUTH_OPEN;
    tick(250);assert(dirty);frame();assert(!wn_gesture);
    unsigned until=ticks+150;while(ticks<until){assert(wa->poll(&in,10));assert(!in.tapped);}
    assert(!text_opens&&!writes&&!connects);stopped();finish();
  }else assert(!"unknown native case");
  printf("Native Wi-Fi case %u %dx%d PASS\n",which,width(),height());return 0;
}
