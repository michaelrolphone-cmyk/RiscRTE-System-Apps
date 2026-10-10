/* Selected paper Wi-Fi list input; keyboard layout remains unchanged. */
static portable_touch_scroll wpv_scroll;
static unsigned wpv_identity,wpv_selected,wpv_submitted_identity,wpv_completed_identity,wpv_submitted_selected,wpv_completed_selected;
static int wpv_submitted_q8,wpv_completed_q8,wpv_hit_q8,wpv_touch_row;
static bool wpv_scroll_dirty,wpv_hit_ready;
static void wpv_scroll_visible(void) {
  if(!paper_token){wpv_completed_identity=wpv_submitted_identity;wpv_completed_q8=wpv_submitted_q8;wpv_completed_selected=wpv_submitted_selected;}
}
void portable_wifi_scroll_reset(void) {
  memset(&wpv_scroll,0,sizeof(wpv_scroll));
  wpv_identity=wpv_submitted_identity=wpv_completed_identity=0;
  wpv_submitted_q8=wpv_completed_q8=0;wpv_selected=0;
  wpv_scroll_dirty=wpv_hit_ready=false;wpv_touch_row=-1;
}
void portable_wifi_scroll_interrupt(void) {
  portable_scroll_cancel(&wpv_scroll);wpv_hit_ready=false;wpv_touch_row=-1;
}
void portable_wifi_scroll_prepare(const portable_wifi_view *v) {
  if(!pp_enabled())return;
  bool changed=wpv_identity!=v->identity;
  if(changed){portable_wifi_scroll_interrupt();wpv_scroll.position_q8=0;wpv_identity=v->identity;}
  portable_scroll_configure(&wpv_scroll,(portable_scroll_viewport){32,WPV_TOP,width()-64,
      height()-168-WPV_TOP},WPV_ROW,v->keyboard?0:(int)v->count);
  if(!v->keyboard && (changed || wpv_selected!=v->selected))portable_scroll_reveal(&wpv_scroll,(int)v->selected);
  wpv_selected=v->selected;
}
bool portable_wifi_scroll_changed(void) {
  bool changed=wpv_scroll_dirty;wpv_scroll_dirty=false;return changed;
}
static bool wifi_scroll_poll(t5_app_input_t *out) {
  if(!pp_enabled())return false;
  portable_touch_sample sample;input_take(&sample);wpv_touch_row=-1;
  if(out->buttons){portable_wifi_scroll_interrupt();return true;}
  if(sample.began){wpv_hit_q8=wpv_scroll.position_q8;wpv_hit_ready=true;}
  unsigned change=portable_scroll_touch(&wpv_scroll,&sample,input_delivered_at);
  if(change&PORTABLE_SCROLL_BACK){out->buttons=T5_APP_BUTTON_BACK;portable_wifi_scroll_interrupt();return true;}
  if((change&PORTABLE_SCROLL_TAP) && wpv_hit_ready) {
    portable_touch_scroll visible=wpv_scroll;visible.position_q8=wpv_hit_q8;
    wpv_touch_row=portable_scroll_row(&visible,sample.x,sample.y);
    out->tapped=true;out->touch_x=(int16_t)sample.x;out->touch_y=(int16_t)sample.y;
  }
  if(change&PORTABLE_SCROLL_CHANGED)wpv_scroll_dirty=true;
  return true;
}

bool portable_wifi_scroll_confirm(const portable_wifi_view *v) {
  if(!pp_enabled() || v->keyboard)return true;
  portable_wifi_scroll_prepare(v);
  if(v->selected>=v->count)return false;
  /* Confirm belongs to the current selection, including one not painted yet. */
  portable_scroll_reveal(&wpv_scroll,(int)v->selected);wpv_scroll_dirty=true;return true;
}
