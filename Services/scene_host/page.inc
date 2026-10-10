/* The scene provider owns chrome, hit targets and immutable raster snapshots. */
static int page_top(void){return nc_size(40);}
static int page_bottom(void){return nc_size(44);}
static int32_t page_geometry(void *c,risc_scene_page_geometry_v1 *g){
 (void)c;if(!alive())return RISC_SCENE_RETAINED;
 if(!started||!g||g->struct_size<sizeof(*g)||width<160||width>1024||height>1024||height<200)return RISC_SCENE_UNAVAILABLE;
 g->width=width&~7u;g->height=height-(unsigned)(page_top()+page_bottom());g->stride=g->width/8u;return RISC_SCENE_OK;
}
static void page_draw(void){
 fill(0,0,(int)width,(int)height,nc_bg());
 upload=(hit_map){.revision=document.revision,.route=current_route(),.epoch=epoch};
 const risc_scene_page_document_v1 *p=&model->page_document;
 int pad=nc_size(16),top=page_top(),bottom=page_bottom();
 nc_text(pad,top-nc_size(13),(int)width-2*pad,p->title,0,nc_fg());
 fill(pad,top-nc_size(5),(int)width-2*pad,nc_scale(),nc_fg());
 unsigned pw=width&~7u,ph=height-(unsigned)(top+bottom),stride=pw/8u;
 const uint8_t *bytes=model==&frame_model?page_frozen:page_live;
 if(!layout_only&&bytes){
  int first=(int)clip_top-top;if(first<0)first=0;
  int last=(int)clip_bottom-top;if(last>(int)ph)last=(int)ph;
  for(int y=first;y<last;y++)for(unsigned x=0;x<pw;x++)
   if(!(bytes[(unsigned)y*stride+x/8u]&(0x80u>>(x%8u))))pixel((int)x,top+y,nc_fg());
 }
 int footer=(int)height-bottom;
 nc_center(pad,footer+nc_size(10),(int)width-2*pad,p->footer,3,nc_fg());
 int w=((int)width-2*pad-nc_size(12))/3;
 const char *labels[]={"PREVIOUS","MENU","NEXT"};
 for(unsigned i=0;i<3;i++){
  const risc_scene_node_v1 *n=&document.nodes[i];int x=pad+(int)i*(w+nc_size(6)),y=footer+nc_size(16),h=nc_size(24);
  nc_box(x,y,w,h,h/2,nc_scale(),nc_fg(),nc_bg());nc_center(x,y+nc_size(16),w,labels[i],5,nc_fg());
  add_hit(x,y,w,h,HIT_ACTION,n->id,0,0);
 }
}
static int32_t page_present(void *c,uint64_t s,const risc_scene_page_document_v1 *p,const uint8_t *pixels,size_t length){
 int32_t state=check(s);if(state)return state;
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};
 if(page_geometry(c,&g)||!p||p->struct_size!=sizeof(*p)||!p->revision||p->revision<=document.revision||!p->previous_action||!p->menu_action||!p->next_action||!p->back_action||!terminated(p->title,sizeof(p->title))||!terminated(p->footer,sizeof(p->footer))||!pixels||length!=(size_t)g.stride*g.height)return RISC_SCENE_INVALID;
 if(!page_live){page_live=malloc(length);page_frozen=malloc(length);if(!page_live||!page_frozen){free(page_live);free(page_frozen);page_live=page_frozen=NULL;return RISC_SCENE_UNAVAILABLE;}page_capacity=length;}
 if(length>page_capacity)return RISC_SCENE_UNAVAILABLE;
 risc_components_document_v1 *d=&page_document_scratch;memset(d,0,sizeof(*d));
 d->api_version=1;d->struct_size=sizeof(*d);d->revision=p->revision;d->root=document.root;d->screen_key=0x50414731;d->route_count=1;d->routes[0].id=d->root;d->routes[0].back_action=p->back_action;d->node_count=3;
 const uint32_t actions[]={p->previous_action,p->menu_action,p->next_action};
 for(unsigned i=0;i<3;i++){d->nodes[i].id=i+1;d->nodes[i].route=d->root;d->nodes[i].kind=RISC_SCENE_ACTION;d->nodes[i].action=actions[i];}
 bool previous_components=model->components;model->components=true;
 int32_t rc=update_document(c,s,d);if(rc){model->components=previous_components;return rc;}
 memcpy(page_live,pixels,length);model->page_document=*p;model->page_view=true;dirty=true;rebuild_hits();return RISC_SCENE_OK;
}
