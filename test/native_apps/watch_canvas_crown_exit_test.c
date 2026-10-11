/* Production Watch canvas adapter, modal controller and cleanup with fakes. */
#define PORTABLE_QUICK_FIXTURE_MAIN original_quick_fixture_main
#include "quick_adapter_test.c"

static bool native_exit_pending, inject_in_controls;
static unsigned native_exit_reads, injected_poll;
static bool native_exit_requested(void) {
  ++native_exit_reads;
  return native_exit_pending;
}
static void yield_native_exit(uint32_t ms) {
  test_yield(ms);
  if(inject_in_controls && quick_modal) {
    inject_in_controls=false;native_exit_pending=true;injected_poll=polls;
    /* A pending UI action must not launch or save after native cancellation. */
    quick.ui.pending|=PQA_WIFI|PQA_VOLUME_COMMIT;
    quick.ui.action_volume=90;
  }
}
static bool named(const char *actual,const char *wanted) {return !strcmp(actual,wanted);}
static void check_exit(const t5_app_input_t *input) {
  assert(input->exit_requested && input->buttons==T5_APP_BUTTON_BACK && !input->tapped);
  assert(!input_pending && !navigation_pending);
  assert(!return_launches && !wifi_launches && !quick_launch_pending);
}
int main(int argc,char **argv) {
  assert(argc==2);const char *test=argv[1];setup();
  quick_runtime.exit_requested=native_exit_requested;
  const t5_app_api_v1 *api=t5_app_get_api(T5_APP_ABI_VERSION);assert(api);
  t5_app_input_t input={0};
  if(named(test,"short-capabilities") || named(test,"short-monotonic")) {
    /* Allocate only the historical prefix: ASan catches a suffix overread. */
    size_t bytes=named(test,"short-capabilities")?RISC_RUNTIME_CAPABILITIES_V1_SIZE:RISC_RUNTIME_MONOTONIC_V1_SIZE;
    risc_runtime_api_v1 *short_api=malloc(bytes);assert(short_api);
    memcpy(short_api,&quick_runtime,bytes);short_api->struct_size=(uint32_t)bytes;rt=short_api;
    assert(api->poll(&input,8) && !input.exit_requested && !native_exit_reads);
    cleanup();free(short_api);
  } else if(named(test,"missing-hook")) {
    quick_runtime.exit_requested=NULL;
    assert(api->poll(&input,8) && !input.exit_requested && !native_exit_reads);
    cleanup();
  } else if(named(test,"legacy-back")) {
    tap(3,20,20);
    for(unsigned n=0;n<10 && !(input.buttons&T5_APP_BUTTON_BACK);++n)assert(api->poll(&input,8));
    assert(input.exit_requested && return_launches==1 && !wifi_launches && native_exit_reads);
    cleanup();
  } else {
    bool inside=named(test,"controls-open") || named(test,"torch-open") || named(test,"slider-open");
    if(inside) {
      quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;
      quick_runtime.yield_ms=yield_native_exit;inject_in_controls=true;
      if(named(test,"torch-open")){quick.ui.torch=true;hardware_brightness=100;}
      if(named(test,"slider-open")) {
        quick.ui.gesture=PQA_BRIGHTNESS_DRAG;quick.ui.saved_brightness=40;
        quick.ui.brightness=80;hardware_brightness=80;
      }
      assert(api->poll(&input,8));check_exit(&input);
      assert(native_exit_pending && !inject_in_controls && polls==injected_poll+1);
      assert(!quick_modal && !quick_background && !pqa_visible(&quick.ui));
      assert(hardware_brightness==40);
      assert(!pref_writes[0] && !pref_writes[1]);
      assert(!memcmp(framebuffer,expected_background,sizeof(expected_background)));
    } else {
      assert(named(test,"pending") || named(test,"alarm") || named(test,"controls") || named(test,"settings"));
      if(named(test,"alarm")){alarm_fake.state=ALARM_STATE_ALERT;alarm_fake.occurrence=(alarm_token_v1){1,7,88,9};}
      if(named(test,"controls")){quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.pending=PQA_WIFI;}
      if(named(test,"settings")){sv_page=SV_VALUE;settings_editing=true;}
      native_exit_pending=true;input_pending=true;navigation_pending=T5_APP_BUTTON_CONFIRM;
      input_sample=(portable_touch_sample){.valid=true,.released=true,.tap_eligible=true};
      unsigned old_polls=polls,old_steps=service_steps,old_ticks=ticks,old_frames=displays;
      assert(api->poll(&input,8));check_exit(&input);
      assert(polls==old_polls && service_steps==old_steps && ticks==old_ticks && displays==old_frames);
      assert(!quick_modal && !quick_background);
    }
    /* Reading never acknowledges the latch or allows a synthetic return. */
    unsigned old_reads=native_exit_reads;
    assert(api->poll(&input,8));check_exit(&input);assert(native_exit_reads==old_reads+1);
    cleanup();assert(native_exit_pending && native_exit_requested());
    assert(!return_launches && !wifi_launches);
  }
  assert(!grants && !subscriptions && !frame_count);
  printf("Watch canvas native crown exit: %s PASS\n",test);
  return 0;
}
