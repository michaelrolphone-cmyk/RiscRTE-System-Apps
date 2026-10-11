/* Immersive document composition, shared by any ui.scene client. */
static int reading_top(void){return nc_size(24);}
static int reading_bottom(void){return nc_size(36);}
static int32_t reading_geometry(void *c,risc_scene_page_geometry_v1 *g){
 int32_t rc=page_geometry(c,g);if(rc)return rc;
 g->height=height-(unsigned)(reading_top()+reading_bottom());return RISC_SCENE_OK;
}
static uint16_t reading_bg(void){return model->reading_document.flags&RISC_SCENE_READING_INVERT?nc_fg():nc_bg();}
static uint16_t reading_fg(void){return model->reading_document.flags&RISC_SCENE_READING_INVERT?nc_bg():nc_fg();}
static void reading_button(int x,int y,unsigned node,unsigned symbol){
 int s=nc_scale();uint16_t fg=reading_fg(),bg=reading_bg();
 nc_box(x,y,28*s,28*s,14*s,nc_paper()?5:s,fg,bg);
 nc_icon(symbol,x+7*s,y+7*s,14*s,fg);
 add_hit(x-3*s,y-3*s,34*s,34*s,HIT_ACTION,node,0,0);
}
static void reading_draw(void){
 const risc_scene_reading_document_v1 *r=&model->reading_document;
 int s=nc_scale(),top=reading_top();unsigned stride=(width&~7u)/8u,ph=height-(unsigned)(top+reading_bottom());
 uint16_t fg=reading_fg(),bg=reading_bg();fill(0,0,(int)width,(int)height,bg);
 upload=(hit_map){.revision=document.revision,.route=current_route(),.epoch=epoch};
 const uint8_t *bytes=model==&frame_model?page_frozen:page_live;
 if(!layout_only&&bytes){
  int first=(int)clip_top-top;if(first<0)first=0;
  int last=(int)clip_bottom-top;if(last>(int)ph)last=(int)ph;
  for(int y=first;y<last;y++)for(unsigned x=0;x<stride*8u;x++)
   if(!(bytes[(unsigned)y*stride+x/8u]&(0x80u>>(x%8u))))pixel((int)x,top+y,fg);
 }
 nc_center(16*s,(int)height-9*s,(int)width-32*s,r->base.footer,5,fg);
 bool chrome=r->flags&RISC_SCENE_READING_CHROME;
 if(chrome||r->flags&RISC_SCENE_READING_SCROLL){add_hit(0,0,(int)width,(int)height,HIT_ACTION,2,0,0);}
 else{
  int edge=(int)width*3/10;
  add_hit(0,0,edge,(int)height,HIT_ACTION,1,0,0);
  add_hit(edge,0,(int)width-2*edge,(int)height,HIT_ACTION,2,0,0);
  add_hit((int)width-edge,0,edge,(int)height,HIT_ACTION,3,0,0);
 }
 if(!chrome)return;
 /* Bars overlay the page; showing them does not repaginate the document. */
 fill(0,0,(int)width,51*s,bg);fill(0,50*s,(int)width,2*s,fg);
 reading_button(14*s,14*s,0,RISC_SYMBOL_DOCUMENT_BACK);
 /* Back uses the copied route, not node zero (which is deliberately invalid). */
 upload.items[upload.count-1].kind=HIT_BACK;
 nc_text(49*s,32*s,(int)width-130*s,r->base.title,1,fg);
 reading_button((int)width-77*s,14*s,4,r->flags&RISC_SCENE_READING_BOOKMARKED?RISC_SYMBOL_BOOKMARK:RISC_SYMBOL_BOOKMARK_OUTLINE);
 reading_button((int)width-42*s,14*s,5,RISC_SYMBOL_SLIDERS);
 int by=(int)height-96*s;fill(0,by,(int)width,96*s,bg);fill(0,by,(int)width,2*s,fg);
 int available=(int)width-28*s-27*s;
 for(unsigned i=0;i<10;i++){
  int left=available*(int)i/10,right=available*(int)(i+1)/10;
  int x=14*s+left+(int)i*3*s,w=right-left;
  bool on=i<(r->progress+50u)/100u;
  nc_box(x,by+10*s,w,18*s,3*s,2*s,fg,on?fg:bg);
  add_hit(x,by+7*s,w,24*s,HIT_VALUE,7,0,(int)i*100+50);
 }
 nc_center(14*s,by+43*s,(int)width-28*s,r->progress_text,3,fg);
 reading_button((int)width/4-14*s,by+52*s,6,RISC_SYMBOL_DOCUMENT_LIST);
 reading_button((int)width*3/4-14*s,by+52*s,5,RISC_SYMBOL_FONT);
}
static int32_t reading_present(void *c,uint64_t session_id,const risc_scene_reading_document_v1 *r,const uint8_t *pixels,size_t length,uint32_t refresh){
 int32_t rc=check(session_id);if(rc)return rc;
 if(closing||paused||pausing)return RISC_SCENE_BUSY;
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};
 if(reading_geometry(c,&g)||!r||r->struct_size!=sizeof(*r)||r->flags&~15u||r->progress>1000||
    r->base.struct_size!=sizeof(r->base)||!r->base.revision||r->base.revision<=document.revision||
    !r->base.previous_action||!r->base.menu_action||!r->base.next_action||!r->base.back_action||
    !r->bookmark_action||!r->layout_action||!r->contents_action||!r->scrub_action||!r->scroll_action||
    !terminated(r->base.title,sizeof(r->base.title))||!terminated(r->base.footer,sizeof(r->base.footer))||
    !terminated(r->progress_text,sizeof(r->progress_text))||refresh>RISC_SCENE_PAGE_REFRESH_CLEAN)return RISC_SCENE_INVALID;
 size_t expected=(size_t)g.stride*g.height;
 if(pixels?length!=expected:(length||!model->reading_view||!page_live))return RISC_SCENE_INVALID;
 if(!page_live){
  size_t capacity=(size_t)width*height/8u;
  page_live=malloc(capacity);page_frozen=malloc(capacity);
  if(!page_live||!page_frozen){free(page_live);free(page_frozen);page_live=page_frozen=NULL;return RISC_SCENE_UNAVAILABLE;}
  memset(page_live,0xff,capacity);memset(page_frozen,0xff,capacity);page_capacity=capacity;
 }
 if(expected>page_capacity)return RISC_SCENE_UNAVAILABLE;
 risc_components_document_v1 *d=&page_document_scratch;memset(d,0,sizeof(*d));
 d->api_version=1;d->struct_size=sizeof(*d);d->revision=r->base.revision;d->root=document.root;
 d->screen_key=RISC_SCENE_READING_TAG;d->route_count=1;d->routes[0].id=d->root;d->routes[0].back_action=r->base.back_action;d->node_count=8;
 const uint32_t actions[]={r->base.previous_action,r->base.menu_action,r->base.next_action,r->bookmark_action,r->layout_action,r->contents_action,r->scrub_action,r->scroll_action};
 for(unsigned i=0;i<8;i++){
  risc_scene_node_v1 *n=&d->nodes[i];n->id=i+1;n->route=d->root;n->action=actions[i];n->kind=RISC_SCENE_ACTION;
  if(i>=6){n->kind=RISC_COMPONENT_STEPPER;n->step=1;n->maximum=i==6?1000:2048;n->value=i==6?(int)r->progress:1024;}
  d->details[i].marker=6;
 }
 bool previous=model->components;model->components=true;
 rc=update_document(c,session_id,d);if(rc){model->components=previous;return rc;}
 bool inversion_changed=model->reading_view&&((model->reading_document.flags^r->flags)&RISC_SCENE_READING_INVERT);
 if(pixels)memcpy(page_live,pixels,length);
 model->reading_document=*r;model->page_document=r->base;model->page_view=true;model->reading_view=true;model->preview_view=false;
 model->page_clean=model->page_clean||refresh==RISC_SCENE_PAGE_REFRESH_CLEAN||inversion_changed;
 dirty=true;rebuild_hits();return RISC_SCENE_OK;
}

static int32_t preview_geometry(void *c,risc_scene_page_geometry_v1 *g){
 int32_t rc=page_geometry(c,g);if(rc)return rc;
 g->width=(width-54u*(unsigned)nc_scale())&~7u;g->height=60u*(unsigned)nc_scale();g->stride=g->width/8u;
 return RISC_SCENE_OK;
}
static int32_t preview_present(void *c,uint64_t session_id,const risc_components_document_v1 *d,uint32_t node,const uint8_t *pixels,size_t length){
 int32_t rc=check(session_id);if(rc)return rc;if(closing||paused||pausing)return RISC_SCENE_BUSY;
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};
 if(preview_geometry(c,&g)||!d||!pixels||length!=(size_t)g.stride*g.height||!valid_document(d,true))return RISC_SCENE_INVALID;
 int at=node_index(d,node);if(at<0||d->nodes[at].kind!=RISC_COMPONENT_TEXT_PREVIEW||d->nodes[at].flags&RISC_SCENE_HIDDEN)return RISC_SCENE_INVALID;
 if(!page_live){
  size_t capacity=(size_t)width*height/8u;page_live=malloc(capacity);page_frozen=malloc(capacity);
  if(!page_live||!page_frozen){free(page_live);free(page_frozen);page_live=page_frozen=NULL;return RISC_SCENE_UNAVAILABLE;}
  memset(page_live,0xff,capacity);memset(page_frozen,0xff,capacity);page_capacity=capacity;
 }
 if(length>page_capacity)return RISC_SCENE_UNAVAILABLE;
 bool previous=model->components;model->components=true;
 rc=update_document(c,session_id,d);if(rc){model->components=previous;return rc;}
 memcpy(page_live,pixels,length);model->preview_view=true;model->preview_node=node;
 model->page_view=model->reading_view=model->page_clean=false;dirty=true;rebuild_hits();return RISC_SCENE_OK;
}
