/* The host owns this scene. Runtime calls it only between completely cleaned
 * invocations, before load/init; the completed panel image then survives until
 * the child presents. No guessed percentage, timer, polling thread or child UI. */
#ifndef RISC_RESIDENT_CALLBACKS_LOADING_V1_SIZE
#error "Host loading requires the optional Runtime loading callback SDK"
#endif
static const t5_app_manifest_t *resident_loading_app(const char *path) {
 if(!path || portable_catalog_count>41)return NULL;
 const char *name=path;
 for(unsigned i=0;i<193 && path[i];++i)if(path[i]=='/')name=path+i+1;
 for(unsigned i=0;i<portable_catalog_count;++i)
  if(!strcmp(name,portable_catalog[i].file_name))return &portable_catalog[i];
 return NULL;
}
static void resident_loading_title(const char *name,int y) {
 char line[96];unsigned used=0,offset=0;
 for(unsigned row=0;row<2;++row) {
  used=0;
  while(offset<95 && name[offset]) {
   unsigned char c=(unsigned char)name[offset];
   if(c<32 || c>126)c='?';
   line[used]=(char)c;line[used+1]=0;
   if(used && pp_measure(line,true)>width()-48)break;
   ++used;++offset;
  }
  line[used]=0;
  if(row==1 && name[offset] && used>=3)memcpy(line+used-3,"...",3);
  int w=pp_measure(line,true);
  pp_text((width()-w)/2,y+(int)row*30,width()-48,line,1,true,true);
  if(!name[offset])break;
 }
}
static int32_t resident_loading(void *context,const char *path) {
 (void)context;
 if(native_custody_retained)return RISC_RESIDENT_RETAINED;
 if(!path || failed || surface.frame || paper_token || !display_settled ||
    alarm_modal || quick_modal || !pp_enabled())return RISC_RESIDENT_BUSY;
#ifdef PORTABLE_PAPER_CROSSFADE
 /* This scene must be legible before loading starts, even on a fast launch. */
 portable_paper_transition_cancel();
#endif
 const t5_app_manifest_t *app=resident_loading_app(path);
 const char *name=app?app->display_name:path;
 if(!app)for(unsigned i=0;i<193 && path[i];++i)if(path[i]=='/')name=path+i+1;
 paper_previous_valid=previous_valid=alarm_pixels_valid=false;
 #ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_immediate())return RISC_RESIDENT_RETAINED;
#endif
 pp_begin();
 if(failed || native_custody_retained || !surface.frame)return RISC_RESIDENT_RETAINED;
 int center=width()/2,top=height()/2-130;
 if(!icon(center-48,top,app?app->icon:"solid:f00a",96,true))
  (void)icon(center-48,top,"solid:f00a",96,true);
 resident_loading_title(name,top+124);
 const char *label="LOADING";
 pp_text((width()-pp_measure(label,false))/2,top+204,width()-48,label,1,false,true);
 /* Dots express indeterminate loading, never measured progress or completion. */
 for(int x=-18;x<=18;x+=18)pp_circle(center+x,top+242,4,true);
 if(failed || native_custody_retained)return RISC_RESIDENT_RETAINED;
 resident_loading_present=true;
 present(true);
 bool settled=!failed && !native_custody_retained && portable_paper_frame_drain() &&
  !surface.frame && display_settled;
 resident_loading_present=false;
 if(!settled) {
  if(!native_custody_retained)portable_adapter_retain();
  return RISC_RESIDENT_RETAINED;
 }
 resident_neutral();
 return RISC_RESIDENT_OK;
}
