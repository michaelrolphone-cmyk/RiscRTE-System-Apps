/* Reusable metric tiles, instruments, binary line graphs and persistent tabs.
 * Domain-free: no vehicle names, PIDs, units, thresholds or polling here. */
#include "fonts/telemetry.inc"
#include "fonts/telemetry_icons.inc"
static const pc_font *tm_font(unsigned role){
 static const pc_font *const f[2][10]={
 {&tmfont_1_0,&tmfont_1_1,&tmfont_1_2,&tmfont_1_3,&tmfont_1_4,&tmfont_1_5,&tmfont_1_6,&tmfont_1_7,&tmfont_1_8,&tmfont_1_9},
 {&tmfont_2_0,&tmfont_2_1,&tmfont_2_2,&tmfont_2_3,&tmfont_2_4,&tmfont_2_5,&tmfont_2_6,&tmfont_2_7,&tmfont_2_8,&tmfont_2_9}};
 return f[nc_scale()-1][role<10?role:0];
}
static int tm_measure(const char *s,unsigned role){return (paper_measure(tm_font(role),s,0)+32)/64;}
static void tm_text(int x,int baseline,int maximum,const char *s,unsigned role,uint16_t color){
 if(layout_only||maximum<=0)return;
 const pc_font *f=tm_font(role);int pen=0;
 for(unsigned k=0;s[k]&&k<72;k++){
  unsigned ch=paper_char((unsigned char)s[k]);const pc_glyph*g=&f->glyphs[ch-32];
  if((pen+g->advance+32)/64>maximum)break;
  for(int row=0;row<g->height;row++){
   int yy=baseline+g->top+row;if(yy<(int)clip_top||yy>=(int)clip_bottom)continue;
   for(int col=0;col<g->width;col++)if(paper_bit(g,col,row))pixel(x+(pen+32)/64+g->left+col,yy,color);
  }pen+=g->advance;
 }
}
static void tm_center(int x,int baseline,int w,const char *s,unsigned role,uint16_t color){int n=tm_measure(s,role);tm_text(x+(w>n?(w-n)/2:0),baseline,w,s,role,color);}
static void tm_icon(unsigned symbol,int x,int y,int size,uint16_t ink){
 if(layout_only||symbol<RISC_SYMBOL_GAUGE||symbol>RISC_SYMBOL_GEAR||size<=0)return;
 const uint32_t *m=tm_icons[symbol-RISC_SYMBOL_GAUGE];
 for(int yy=0;yy<size;yy++)for(int xx=0;xx<size;xx++)if(m[yy*32/size]&(UINT32_C(1)<<(31-xx*32/size)))pixel(x+xx,y+yy,ink);
}
static int tm_layout(unsigned target,int *ox,int *oy,int *ow,int *oh){
 int s=nc_scale(),y=nc_header_height(),x=16*s,w=(int)width-32*s,col=0,tile_kind=0,tile_step=51*s,chip_x=16*s;bool chips=false;
 for(unsigned i=0;i<document.node_count;i++){
  const risc_scene_node_v1*n=&document.nodes[i];if(n->route!=current_route()||n->flags&RISC_SCENE_HIDDEN||n->kind==RISC_COMPONENT_HEADER_ACTION||n->kind==RISC_COMPONENT_TAB_BAR)continue;
  int cw=w,ch=nc_row_height(n);
  if(n->kind==RISC_COMPONENT_METRIC_TILE||n->kind==RISC_COMPONENT_STATUS_TILE){
   if(col&&tile_kind!=(int)n->kind){y+=tile_step;col=0;}
   tile_kind=(int)n->kind;tile_step=(n->kind==RISC_COMPONENT_METRIC_TILE?51:32)*s;
   if(chips){y+=34*s;chips=false;}x=14*s+col*((int)width-23*s)/2;cw=((int)width-33*s)/2;ch=tile_step-5*s;
  }else {
   if(col){y+=tile_step;col=0;}x=16*s;
   if(n->kind==RISC_COMPONENT_CHIP){
    if(!chips){y+=4*s;chips=true;chip_x=16*s;}
    x=chip_x;cw=nc_measure(1,n->label)+24*s;if(cw>w)cw=w;
    if(x+cw>(int)width-16*s){y+=36*s;x=16*s;}ch=30*s;chip_x=x+cw+6*s;
   }else if(chips){y+=34*s;chips=false;}
   if(n->kind==RISC_COMPONENT_INSTRUMENT||n->kind==RISC_COMPONENT_BAR_GAUGE){x=14*s;cw=(int)width-28*s;}
  }
  if(i==target){*ox=x;*oy=y;*ow=cw;*oh=ch;return y+ch;}
  if(n->kind==RISC_COMPONENT_METRIC_TILE||n->kind==RISC_COMPONENT_STATUS_TILE){col^=1;if(!col)y+=tile_step;}
  else if(!chips)y+=ch;
 }
 *ox=x;*oy=y;*ow=w;*oh=0;return y+(col?tile_step:0)+(chips?36*s:0);
}
static void tm_path(const risc_scene_curve_v1*c,int x,int y,int w,int h,uint16_t ink,int stroke){
 if(!c||layout_only||c->count<2)return;
 for(unsigned i=1;i<c->count;i++){
  if(c->points[i-1]>10000||c->points[i]>10000)continue;
  int x0=x+(int)(i-1)*w/(int)(c->count-1),x1=x+(int)i*w/(int)(c->count-1);
  int y0=y+h-(int)c->points[i-1]*h/10000,y1=y+h-(int)c->points[i]*h/10000;
  /* A dash pattern in screen-x coordinates stays identifiable after zoom. */
  for(int xx=x0;xx<=x1;xx++){
   if(c->style==1&&(xx-x)%(10*nc_scale())>=7*nc_scale())continue;
   if(c->style==2&&(xx-x)%(5*nc_scale())>=2*nc_scale())continue;
   int yy=y0+(x1==x0?0:(y1-y0)*(xx-x0)/(x1-x0));
   int next=y0+(x1==x0?0:(y1-y0)*((xx<x1?xx+1:xx)-x0)/(x1-x0));
   nc_line(xx,yy,xx,next,stroke,ink);
  }
 }
}
static void tm_tabs(void){
 int s=nc_scale(),y=(int)height-36*s;
 for(unsigned i=0;i<document.node_count;i++){
  const risc_scene_node_v1*n=&document.nodes[i];if(n->route!=current_route()||n->kind!=RISC_COMPONENT_TAB_BAR||n->flags&RISC_SCENE_HIDDEN)continue;
  fill(0,y,(int)width,36*s,nc_bg());fill(0,y,(int)width,2*s,nc_fg());
  for(unsigned j=0;j<4;j++){
   int x=(int)width*(int)j/4,w=(int)width/4;bool on=n->value==(int)j;char label[24];nc_choice(document.details[i].choices,j,label,sizeof(label));
   uint16_t fg=on?nc_bg():nc_fg();if(on)fill(x,y+2*s,w,34*s,nc_fg());
   int tw=tm_measure(label,1),total=tw+16*s,left=x+(w-total)/2;
   tm_icon(RISC_SYMBOL_GAUGE+j,left,y+13*s,12*s,fg);tm_text(left+16*s,y+22*s,w-16*s,label,1,fg);
   if(j==2&&document.details[i].badge[0]){
    nc_round(x+w-20*s,y+3*s,14*s,12*s,6*s,fg);tm_center(x+w-20*s,y+12*s,14*s,document.details[i].badge,1,on?nc_fg():nc_bg());
   }
   nc_hit(x,y,w,36*s,HIT_VALUE,n,(int)j,true);
  }
 }
}
static void tm_header(void){
 int s=nc_scale(),left=14*s,right=(int)width-14*s,hh=nc_header_height(),r=route_index(&document,current_route());
 fill(0,0,(int)width,hh,nc_bg());unsigned header_count=0;
 for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].route==current_route()&&document.nodes[i].kind==RISC_COMPONENT_HEADER_ACTION&&!(document.nodes[i].flags&RISC_SCENE_HIDDEN)){
  const risc_scene_node_v1*n=&document.nodes[i];int xx=right-28*s;right-=35*s;header_count++;
  nc_box(xx,13*s,28*s,28*s,14*s,5*s/2,nc_fg(),nc_bg());nc_icon(document.details[i].symbol,xx+7*s,20*s,14*s,nc_fg());nc_hit(xx-3*s,10*s,34*s,34*s,HIT_ACTION,n,0,true);
 }
 if(r>=0&&strchr(document.subtitle,'|')){
  tm_text(left,31*s,right-left,document.routes[r].title,0,nc_fg());int xx=left+tm_measure(document.routes[r].title,0)+7*s;
  for(unsigned j=0;j<2;j++){char tag[32];nc_choice(document.subtitle,j,tag,sizeof(tag));int tw=tm_measure(tag,2),ww=tw+10*s;if(xx+ww>right)break;
   nc_box(xx,20*s,ww,14*s,6*s,2*s,nc_fg(),nc_bg());tm_text(xx+5*s,30*s,tw,tag,2,nc_fg());xx+=ww+6*s;
  }
 }else if(r>=0){tm_text(left,31*s,right-left,document.routes[r].title,0,nc_fg());
  if(document.subtitle[0])tm_text(left,44*s,right-left,document.subtitle,1,nc_fg());}
 fill(header_count?0:16*s,hh-2*s,header_count?(int)width:(int)width-32*s,2*s,nc_fg());
}
static bool tm_draw(unsigned index,int x,int y,int w,int h){
 const risc_scene_node_v1*n=&document.nodes[index];const risc_component_detail_v1*d=&document.details[index];
 if(n->kind==RISC_COMPONENT_CARD&&(document.flags&RISC_COMPONENTS_TELEMETRY)&&d->symbol){
  int scale=nc_scale();bool inverse=n->flags&RISC_SCENE_PRIMARY;uint16_t ink=inverse?nc_bg():nc_fg();
  nc_box(x,y+8*scale,w,h-16*scale,12*scale,5*scale/2,nc_fg(),inverse?nc_fg():nc_bg());
  nc_icon(d->symbol,x+11*scale,y+25*scale,22*scale,ink);
  nc_text(x+43*scale,y+30*scale,w-54*scale,n->label,0,ink);nc_text(x+43*scale,y+47*scale,w-54*scale,n->text,3,ink);return true;
 }
 if(!tm_kind(n->kind))return false;
 if(n->kind==RISC_COMPONENT_TAB_BAR)return true;
 int s=nc_scale();uint16_t fg=nc_fg(),bg=nc_bg();char t[40];
 if(n->kind==RISC_COMPONENT_INSTRUMENT){
  if(d->tone==RISC_TONE_DANGER){nc_round(x,y+6*s,w,54*s,10*s,fg);tm_center(x,y+37*s,w,n->text,1,bg);return true;}
  tm_text(x,y+52*s,w*3/4,n->label,3,fg);int tw=tm_measure(n->label,3);tm_text(x+tw+5*s,y+52*s,w-tw,n->text,1,fg);
  for(unsigned j=0;j<2;j++){nc_choice(d->choices,j*2,t,sizeof(t));tm_center(x+w-55*s,y+(12+(int)j*26)*s,55*s,t,2,fg);nc_choice(d->choices,j*2+1,t,sizeof(t));tm_center(x+w-55*s,y+(26+(int)j*26)*s,55*s,t,tm_measure(t,0)<=55*s?0:1,fg);}return true;
 }
 if(n->kind==RISC_COMPONENT_BAR_GAUGE){
  tm_text(x,y+9*s,w/2,n->label,1,fg);tm_text(x+w-tm_measure(n->text,7),y+12*s,w/2,n->text,7,fg);
  int by=y+17*s,bh=16*s;nc_box(x,by,w,bh,8*s,5*s/2,fg,bg);
  int amount=n->maximum?(w-4*s)*n->value/n->maximum:0;if(amount)nc_progress_fill(x+2*s,by+2*s,w-4*s,bh-4*s,6*s,amount,fg);
  int red=x+w*13/16;
  for(int yy=by+3*s;yy<by+bh-3*s;yy++)for(int xx=red;xx<x+w-3*s;xx++)if((xx+yy)%(7*s)<2*s)pixel(xx,yy,fg);else pixel(xx,yy,bg);
  for(unsigned i=0;i<=8;i++){char b[4];snprintf(b,sizeof(b),"%u",i);tm_text(x+(w-4*s)*(int)i/8,y+43*s,12*s,b,2,fg);}return true;
 }
 if(n->kind==RISC_COMPONENT_METRIC_TILE){
  bool hot=d->tone==RISC_TONE_DANGER;uint16_t ink=hot?bg:fg;nc_box(x,y,w,h,7*s,5*s/2,fg,hot?fg:bg);
  tm_text(x+7*s,y+12*s,w-14*s,n->label,2,ink);tm_text(x+7*s,y+33*s,w-12*s,n->text,4,ink);
  int tw=tm_measure(n->text,4);tm_text(x+8*s+tw,y+33*s,w-tw-12*s,d->badge,5,ink);
  for(unsigned i=0;i<model->telemetry.count;i++)if(model->telemetry.curves[i].node==n->id)tm_path(&model->telemetry.curves[i],x+w-44*s,y+h-18*s,38*s,12*s,ink,s);
  if(d->tone==RISC_TONE_MUTED)for(int yy=y+8*s;yy<y+h-8*s;yy+=4*s)fill(x,yy,2*s,2*s,bg);
  if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);
  return true;
 }
 if(n->kind==RISC_COMPONENT_CODE_ROW){
  fill(x,y,3*s,h,fg);fill(x,y+h-s,w,s,fg);tm_text(x+11*s,y+24*s,61*s,n->label,7,fg);
  const char*text=n->text;int right=x+79*s,avail=w-87*s;
  for(unsigned line=0;line<3&&*text;line++){
   char part[72];unsigned nchar=0,last_space=0;while(text[nchar]&&nchar+1<sizeof(part)){part[nchar]=text[nchar];part[nchar+1]=0;if(tm_measure(part,8)>avail)break;if(text[nchar]==' ')last_space=nchar;nchar++;}
   if(text[nchar]&&last_space)nchar=last_space;
   if(!nchar)break;
   memcpy(part,text,nchar);part[nchar]=0;tm_text(right,y+(13+(int)line*12)*s,avail,part,8,fg);text+=nchar;while(*text==' ')text++;
  }
  tm_text(x+11*s,y+48*s,w-20*s,d->choices,2,fg);if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);return true;
 }
 if(n->kind==RISC_COMPONENT_VALUE_HERO){
  tm_center(x,y+47*s,w,n->label,6,fg);tm_center(x,y+65*s,w,n->text,1,fg);return true;
 }
 if(n->kind==RISC_COMPONENT_STAT_TRIPLE){
  for(unsigned j=0;j<3;j++){int xx=x+(w+8*s)*(int)j/3,ww=(w-16*s)/3;nc_box(xx,y,ww,h-3*s,6*s,2*s,fg,bg);nc_choice(d->choices,j,t,sizeof(t));tm_center(xx,y+10*s,ww,t,2,fg);nc_choice(n->text,j,t,sizeof(t));tm_center(xx,y+23*s,ww,t,1,fg);}return true;
 }
 if(n->kind==RISC_COMPONENT_TRACE_LEGEND){
  for(int xx=0;xx<30*s;xx++)if(n->value==0||(n->value==1?xx%(10*s)<7*s:xx%(5*s)<2*s))fill(x+xx,y+12*s,s,2*s,fg);
  int right=x+w,range=tm_measure(d->choices,2),unit=tm_measure(d->badge,1),val=tm_measure(n->text,9);
  right-=range;tm_text(right,y+16*s,range,d->choices,2,fg);right-=unit+5*s;tm_text(right,y+16*s,unit,d->badge,1,fg);right-=val+5*s;tm_text(right,y+16*s,val,n->text,9,fg);
  tm_text(x+36*s,y+16*s,right-x-41*s,n->label,8,fg);fill(x,y+h-s,w,s,fg);if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);return true;
 }
 if(n->kind==RISC_COMPONENT_STATUS_TILE){
  nc_box(x,y,w,h,6*s,2*s,fg,bg);int xx=x+6*s,yy=y+6*s;
  if(n->value==0){nc_line(xx,yy+6*s,xx+3*s,yy+9*s,2*s,fg);nc_line(xx+3*s,yy+9*s,xx+9*s,yy,2*s,fg);}
  else if(n->value==1){nc_line(xx,yy,xx+8*s,yy+10*s,s,fg);nc_line(xx+8*s,yy,xx,yy+10*s,s,fg);fill(xx,yy,8*s,s,fg);fill(xx,yy+10*s,8*s,s,fg);for(int j=0;j<w;j+=6*s)fill(x+j,y,2*s,2*s,bg);}
  else fill(xx,yy+5*s,8*s,s,fg);
  tm_text(x+20*s,y+17*s,w-24*s,n->label,1,fg);return true;
 }
 if(n->kind==RISC_COMPONENT_LOG_LINE){
  bool outgoing=n->text[0]=='>';if(outgoing)fill(x,y,w,14*s,fg);
  nc_text(x+2*s,y+11*s,w-4*s,n->text,3,outgoing?bg:fg);return true;
 }
 if(n->kind==RISC_COMPONENT_PLOT){
  int bx=x,by=y+4*s,bw=w,bh=150*s,l=d->choices[0]?24*s:6*s,r=6*s,top=8*s,bottom=14*s;
  nc_box(bx,by,bw,bh,4*s,5*s/2,fg,bg);int px=bx+l,py=by+top,pw=bw-l-r,ph=bh-top-bottom;
  for(int j=0;j<=4;j++)for(int xx=0;xx<pw;xx+=4*s)fill(px+xx,py+ph*j/4,s,s,fg);
  for(int j=0;j<=3;j++){
   int xx=px+pw*j/3;for(int yy=0;yy<ph;yy+=4*s)fill(xx,py+yy,s,s,fg);
   char label[32];if(j==3)snprintf(label,sizeof(label),"NOW");else snprintf(label,sizeof(label),"-%ldS",(long)(n->value < 0 ? 0 : n->value > 600 ? 600 : n->value)*(3-j)/3);
   int tx=j==3?xx-tm_measure(label,2):j==0?xx:xx-tm_measure(label,2)/2;tm_text(tx,by+bh-3*s,50*s,label,2,fg);
  }
  if(d->choices[0]){nc_choice(d->choices,0,t,sizeof(t));tm_text(bx+2*s,py+4*s,l-4*s,t,2,fg);nc_choice(d->choices,1,t,sizeof(t));tm_text(bx+2*s,py+ph,l-4*s,t,2,fg);}
  for(unsigned i=0;i<model->telemetry.count;i++)if(model->telemetry.curves[i].node==n->id)tm_path(&model->telemetry.curves[i],px,py,pw,ph,fg,2*s);
  for(unsigned j=0;j<2;j++)if(n->step&(1<<j)){
   int yy=py+ph-(j?n->minimum:n->maximum)*ph/10000;for(int xx=0;xx<pw;xx+=9*s)fill(px+xx,yy,6*s,2*s,fg);tm_text(px+pw-28*s,yy-2*s,28*s,"LIMIT",2,fg);
  }return true;
 }
 return true;
}
static int32_t telemetry_present(void*c,uint64_t s,const risc_components_document_v1*d,const risc_scene_telemetry_v1*p){
 int32_t rc=check(s);if(rc)return rc;
 if(!model->components||!d||!(d->flags&RISC_COMPONENTS_TELEMETRY)||!valid_document(d,true)||!p||p->struct_size!=sizeof(*p)||p->count>RISC_TELEMETRY_CURVES)return RISC_SCENE_INVALID;
 for(unsigned i=0;i<p->count;i++){
  const risc_scene_curve_v1*q=&p->curves[i];int at=node_index(d,q->node);
  if(at<0||(d->nodes[at].kind!=RISC_COMPONENT_PLOT&&d->nodes[at].kind!=RISC_COMPONENT_METRIC_TILE)||q->style>2||q->count>RISC_TELEMETRY_POINTS)return RISC_SCENE_INVALID;
  unsigned same=0;for(unsigned j=0;j<i;j++)if(p->curves[j].node==q->node){same++;if(p->curves[j].style==q->style)return RISC_SCENE_INVALID;}
  if(same>=3||(same&&d->nodes[at].kind==RISC_COMPONENT_METRIC_TILE))return RISC_SCENE_INVALID;
  for(unsigned j=0;j<q->count;j++)if(q->points[j]>10000&&q->points[j]!=RISC_TELEMETRY_GAP)return RISC_SCENE_INVALID;
 }
 rc=update_document(c,s,d);if(rc)return rc;
 model->telemetry=*p;model->page_view=model->reading_view=model->preview_view=model->page_clean=false;dirty=true;rebuild_hits();return RISC_SCENE_OK;
}
