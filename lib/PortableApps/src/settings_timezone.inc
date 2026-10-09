/* Explicit, app-owned namespace-1 preference editor. This selector does not
 * change RTC interpretation or civil fields: integration is a separate opt-in. */
static void stz_open_cities(void) {
  portable_perf_action(PORTABLE_PERF_TIMEZONE_OPEN,false);
  portable_stage_log(rt,"action","name=timezone-open");
  stz_region=stz_selected;stz_selected=0;
  for(unsigned i=0;i<portable_timezone_region_count(stz_region);++i)
    if(portable_timezone_city_index(stz_region,i)==(int)stz_choice){stz_selected=i;break;}
  sv_page=SV_TIMEZONE_CITIES;stz_choose(stz_selected);sv_switch(SV_TIMEZONE_CITIES);
}
static void stz_back_regions(void) {
  stz_selected=stz_region;stz_visible();sv_switch(SV_TIMEZONE_REGIONS);
}
static uint8_t stz_activate(void) {
  if(!stz_unconfirmed) {
    char id[PORTABLE_TIMEZONE_ID_BYTES];int loaded=portable_timezone_preference_load(settings_store,id);
    stz_choice=(unsigned)portable_timezone_find(id,sizeof(id));stz_status=loaded;
    stz_message=loaded==PORTABLE_TIMEZONE_UNAVAILABLE?"Storage unavailable":
        loaded==PORTABLE_TIMEZONE_INVALID_RECORD?"Invalid record - choose & Save":
        loaded==PORTABLE_TIMEZONE_MISSING?"UTC default (not saved)":"";
  } else stz_message="Save unconfirmed - retry";
  stz_region=(unsigned)portable_timezone_get(stz_choice)->region;
  stz_selected=stz_region;stz_visible();sv_switch(SV_TIMEZONE_REGIONS);
  bool armed=false;uint8_t result=T5_APP_SETTING_NO_CHANGE;t5_app_input_t input;
  while(poll(&input,20)) {
    if(input.exit_requested)break;
    if(!input.buttons&&!input.tapped){armed=true;continue;}
    if(!armed)continue;
    armed=false;
    bool cities=sv_page==SV_TIMEZONE_CITIES,back=false,cancel=false,confirm=false,redraw=true;
    if(input.buttons&T5_APP_BUTTON_BACK)back=true;
    else if(input.buttons&T5_APP_BUTTON_UP)stz_choose((stz_selected+stz_count()-1)%stz_count());
    else if(input.buttons&T5_APP_BUTTON_DOWN)stz_choose((stz_selected+1)%stz_count());
    else if(input.buttons&T5_APP_BUTTON_LEFT)redraw=stz_page_step(false);
    else if(input.buttons&T5_APP_BUTTON_RIGHT)redraw=stz_page_step(true);
    else if(input.buttons&T5_APP_BUTTON_CONFIRM)confirm=true;
    else if(input.tapped) {
      int x=input.touch_x,y=input.touch_y,w=stz_width(),h=stz_height(),margin=stz_margin();
      bool paper=pp_enabled();int page_y=paper?h-192:178,page_h=paper?56:24,page_w=paper?112:60;
      int foot_y=paper?h-112:210,foot_h=paper?88:28,gap=paper?12:6;
      int columns=cities?3:2,foot_w=(w-2*margin-(columns-1)*gap)/columns;
      if(x>=margin&&x<w-margin) {
        if(y>=stz_top()&&y<stz_top()+(int)stz_rows()*stz_step()) {
          unsigned row=stz_first+(unsigned)(y-stz_top())/(unsigned)stz_step();
          if(row<stz_count()){stz_choose(row);if(!cities)confirm=true;}
        } else if(y>=page_y&&y<page_y+page_h) {
          if(x<margin+page_w)redraw=stz_page_step(false);
          else if(x>=w-margin-page_w)redraw=stz_page_step(true);
        } else if(y>=foot_y&&y<foot_y+foot_h) {
          int slot=(x-margin)/(foot_w+gap);
          if((x-margin)%(foot_w+gap)<foot_w) {
            if(slot==columns-1)confirm=true;
            else if(cities&&slot==0)back=true;
            else cancel=true;
          }
        }
      }
    }
    if(cancel)break;
    if(back){if(!cities)break;stz_back_regions();continue;}
    if(confirm) {
      if(!cities){stz_open_cities();continue;}
      const char *id=portable_timezone_get(stz_choice)->id;
      int saved=portable_timezone_preference_save(settings_store,id,PORTABLE_TIMEZONE_ID_BYTES);
      if(saved>=0) {
        stz_unconfirmed=false;result=T5_APP_SETTING_UPDATED;
        stz_message=saved==PORTABLE_TIMEZONE_VIRTUAL_DEFAULT?"UTC default (not saved)":
            saved==PORTABLE_TIMEZONE_UNCHANGED?"Time zone confirmed":"Time zone saved";
        break;
      }
      if(saved==PORTABLE_TIMEZONE_SAVE_UNAVAILABLE) {
        /* No put occurred. Keep any earlier uncertain commit visible. */
        stz_status=PORTABLE_TIMEZONE_UNAVAILABLE;
        stz_message=stz_unconfirmed?"Unconfirmed; storage unavailable":"Storage unavailable";
      } else if(saved==PORTABLE_TIMEZONE_WRITE_FAILED || saved==PORTABLE_TIMEZONE_VERIFY_FAILED) {
        stz_unconfirmed=true;stz_message="Save unconfirmed - retry";
      } else stz_message="Invalid selection";
    }
    if(redraw)settings_view_redraw();
  }
  settings_message=stz_unconfirmed?"Time zone unconfirmed":stz_message;
  sv_switch(SV_ROOT);return result;
}
