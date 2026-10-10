/* NOVA-7 components. All geometry, type, palette, touch regions, scrolling,
 * modal arbitration and density adaptation live here, never in an app. */
#include "fonts/components.inc"
static bool nc_paper(void){return profile->preferred_format!=RISC_DISPLAY_FORMAT_RGB565;}
static int nc_scale(void){return width>=400?2:1;}
static int nc_size(int n){return n*nc_scale();}
static uint16_t nc_bg(void){return nc_paper()?0xffff:0;}
static uint16_t nc_fg(void){return nc_paper()?0:0xffff;}
static uint16_t nc_color(unsigned tone){
    if(nc_paper())return 0;
    static const uint16_t colors[]={0xffff,0x1f1f,0x3ff3,0xfd84,0xfb4b,0x6c11};
    return colors[tone<=RISC_TONE_MUTED?tone:0];
}
static const pc_font *nc_font(unsigned role){
    static const pc_font *const faces[2][5]={{&nova_title_1,&nova_section_1,&nova_body_1,&nova_small_1,&nova_value_1},{&nova_title_2,&nova_section_2,&nova_body_2,&nova_small_2,&nova_value_2}};
    return faces[nc_scale()-1][role];
}
static int nc_measure(unsigned role,const char *s){return (paper_measure(nc_font(role),s,0)+32)/64;}
static void nc_text(int x,int baseline,int maximum,const char *s,unsigned role,uint16_t color){
    if(layout_only||maximum<=0)return;
    const pc_font *f=nc_font(role);int pen=0;
    for(unsigned k=0;s[k]&&k<RISC_SCENE_TEXT;k++){
        unsigned ch=paper_char((unsigned char)s[k]);const pc_glyph *g=&f->glyphs[ch-32];
        if((pen+g->advance+32)/64>maximum){
            /* Truncate at glyph boundaries; never squash a long task name. */
            if(maximum>nc_size(8))fill(x+maximum-nc_size(6),baseline-nc_size(2),nc_size(4),nc_size(1),color);
            break;
        }
        for(int row=0;row<g->height;row++){
            int yy=baseline+g->top+row;if(yy<(int)clip_top||yy>=(int)clip_bottom)continue;
            for(int col=0;col<g->width;col++)if(paper_bit(g,col,row))pixel(x+(pen+32)/64+g->left+col,yy,color);
        }
        pen+=g->advance;
    }
}
static void nc_center(int x,int y,int w,const char *s,unsigned role,uint16_t color){int n=nc_measure(role,s);nc_text(x+(n<w?(w-n)/2:0),y,w,s,role,color);}
static void nc_round(int x,int y,int w,int h,int radius,uint16_t color){
    if(layout_only||w<=0||h<=0)return;
    for(int row=0;row<h;row++){
        if(y+row<(int)clip_top||y+row>=(int)clip_bottom)continue;
        int dy=row<radius?radius-row-1:row>=h-radius?row-h+radius:0,inset=0;
        if(dy)while(inset<radius&&(radius-inset)*(radius-inset)+dy*dy>radius*radius)++inset;
        fill(x+inset,y+row,w-2*inset,1,color);
    }
}
static void nc_box(int x,int y,int w,int h,int r,int stroke,uint16_t ink,uint16_t bg){nc_round(x,y,w,h,r,ink);nc_round(x+stroke,y+stroke,w-2*stroke,h-2*stroke,r>stroke?r-stroke:0,bg);}
static void nc_line(int x,int y,int xx,int yy,int stroke,uint16_t c){
    if(layout_only)return;
    int dx=paper_abs(xx-x),dy=-paper_abs(yy-y),sx=x<xx?1:-1,sy=y<yy?1:-1,err=dx+dy;
    for(;;){fill(x-stroke/2,y-stroke/2,stroke,stroke,c);if(x==xx&&y==yy)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
static void nc_icon(unsigned symbol,int x,int y,int size,uint16_t c){
    int a=size/4,b=size*3/4,m=size/2,st=nc_scale();
    switch(symbol){
    case RISC_SYMBOL_PLUS:nc_line(x+m,y+a,x+m,y+b,st,c);nc_line(x+a,y+m,x+b,y+m,st,c);break;
    case RISC_SYMBOL_MORE:for(int i=0;i<3;i++)nc_round(x+a+i*a-1,y+m-1,2*st,2*st,st,c);break;
    case RISC_SYMBOL_BACK:nc_line(x+b,y+a,x+a,y+m,st,c);nc_line(x+a,y+m,x+b,y+b,st,c);break;
    case RISC_SYMBOL_CHECK:nc_line(x+a,y+m,x+m,y+b,2*st,c);nc_line(x+m,y+b,x+size,y+a,2*st,c);break;
    case RISC_SYMBOL_CLOCK:case RISC_SYMBOL_TODAY:nc_box(x+a/2,y+a/2,size-a,size-a,m,st,c,nc_bg());nc_line(x+m,y+a,x+m,y+m,st,c);nc_line(x+m,y+m,x+b,y+m,st,c);break;
    case RISC_SYMBOL_EDIT:nc_line(x+a,y+b,x+b,y+a,3*st,c);break;
    case RISC_SYMBOL_DELETE:nc_box(x+a,y+a,size/2,size*2/3,st,st,c,nc_bg());nc_line(x+a/2,y+a,x+size-a/2,y+a,st,c);break;
    default:for(int i=0;i<3;i++){fill(x+a/2,y+a+i*a,st,st,c);fill(x+a,y+a+i*a,size-a,st,c);}break;
    }
}
static void nc_marker(unsigned marker,int x,int y,int size,bool inverse){
    static const unsigned tones[]={1,2,3,4,1,2};uint16_t c=inverse?nc_bg():nc_color(tones[marker%6]);
    int mid=size/2,st=nc_scale();
    switch(marker%6){
    case 0:nc_round(x,y,size,size,mid,c);break;
    case 1:fill(x,y,size,size,c);break;
    case 2:for(int r=0;r<size;r++)fill(x+mid-r/2,y+r,r+1,1,c);break;
    case 3:for(int r=0;r<size;r++){int span=mid-paper_abs(mid-r);fill(x+mid-span,y+r,2*span+1,1,c);}break;
    case 4:fill(x+mid-st,y,2*st,size,c);fill(x,y+mid-st,size,2*st,c);break;
    default:nc_box(x,y,size,size,mid,2*st,c,inverse?nc_fg():nc_bg());break;
    }
}
static int nc_header_height(void){return nc_size(document.flags?40:document.subtitle[0]?64:48);}
static int nc_row_height(const risc_scene_node_v1 *n){
    switch(n->kind){
    case RISC_COMPONENT_HEADER_ACTION:return 0;
    case RISC_COMPONENT_SECTION:return nc_size(28);
    case RISC_COMPONENT_PROGRESS:return nc_size(n->label[0]?72:34);
    case RISC_COMPONENT_EMPTY:return nc_size(document.flags?56:96);
    case RISC_COMPONENT_STEPPER:return nc_size(64);
    case RISC_COMPONENT_TIME_PICKER:return nc_size(nc_paper()?188:96);
    case RISC_COMPONENT_MARKERS:return nc_size(88);
    case RISC_SCENE_ACTION:return nc_size(nc_paper()?46:42);
    case RISC_SCENE_TEXT_NODE:return nc_size(56);
    default:return nc_size(nc_paper()?48:44);
    }
}
static void nc_hit(int x,int y,int w,int h,uint32_t kind,const risc_scene_node_v1 *n,int value,bool header){
    if(n&&(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)))return;
    int miny=nc_paper()&&!header?nc_header_height():0;
    if(y<miny){h-=miny-y;y=miny;}if(y+h>(int)height)h=(int)height-y;
    if(w>0&&h>0)add_hit(x,y,w,h,kind,n?n->id:0,0,value);
}
static void nc_control(int x,int y,int w,int h,const char *label,bool selected,uint32_t kind,const risc_scene_node_v1 *n,int value){
    uint16_t ink=nc_color(n&&n->flags&RISC_SCENE_DESTRUCTIVE?RISC_TONE_DANGER:RISC_TONE_ACCENT),bg=nc_bg(),fg=nc_fg();
    if(n&&n->flags&RISC_SCENE_DISABLED){ink=nc_color(RISC_TONE_MUTED);fg=ink;}
    nc_box(x,y,w,h,nc_size(5),nc_size(nc_paper()?2:1),ink,selected?ink:bg);
    nc_center(x,y+h/2+nc_size(4),w,label,1,selected?bg:fg);
    if(n&&n->id==path.focus[path.depth-1])fill(x,y+h-nc_size(3),w,nc_size(3),ink);
    nc_hit(x,y,w,h,kind,n,value,false);
}
static void nc_choice(const char *all,unsigned index,char *out,size_t size){
    while(index&&*all){if(*all++=='|')--index;}size_t i=0;
    while(*all&&*all!='|'&&i+1<size)out[i++]=*all++;
    out[i]=0;
}
static void nc_draw_node(unsigned index,int y){
    const risc_scene_node_v1 *n=&document.nodes[index];const risc_component_detail_v1 *d=&document.details[index];
    int s=nc_scale(),x=16*s,w=(int)width-32*s,h=nc_row_height(n);
    if(y+h<0||y>(int)height)return;
    uint16_t fg=nc_fg(),dim=nc_color(RISC_TONE_MUTED),accent=nc_color(d->tone?d->tone:RISC_TONE_ACCENT);
    if(n->flags&RISC_SCENE_DISABLED){fg=dim;accent=dim;}
    if(n->kind==RISC_COMPONENT_SECTION){nc_text(x,y+20*s,w,n->label,1,dim);return;}
    if(n->kind==RISC_COMPONENT_PROGRESS){
        if(n->label[0]){nc_box(x,y+4*s,w,h-8*s,5*s,s,accent,nc_bg());nc_text(x+10*s,y+24*s,w-20*s,n->label,0,accent);nc_text(x+10*s,y+40*s,w-20*s,n->text,3,dim);}
        int bx=x+(n->label[0]?10*s:0),bw=w-(n->label[0]?20*s:0),by=y+h-18*s;
        nc_round(bx,by,bw,6*s,3*s,nc_paper()?nc_fg():0x0925);
        int filled=n->maximum>0?(bw*n->value/n->maximum):0;
        if(nc_paper())nc_round(bx+s,by+s,bw-2*s,4*s,2*s,nc_bg());
        if(filled>0)nc_round(bx,by,filled,6*s,3*s,accent);
        if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);
        return;
    }
    if(n->kind==RISC_COMPONENT_EMPTY||n->kind==RISC_SCENE_TEXT_NODE){
        int yy=y+(n->kind==RISC_COMPONENT_EMPTY?34:18)*s;
        if(n->kind==RISC_COMPONENT_EMPTY)nc_icon(RISC_SYMBOL_CHECK,(int)width/2-10*s,y+6*s,16*s,accent);
        nc_center(x,yy,w,n->label,1,fg);
        /* Wrap explanatory content on spaces, with body-size glyphs. */
        const char *p=n->text;while(*p&&yy<y+h-8*s){char line[72];unsigned count=0,space=0;while(p[count]&&count<70){line[count]=p[count];line[count+1]=0;if(nc_measure(3,line)>w-8*s)break;if(p[count]==' ')space=count;++count;}if(p[count]&&space)count=space;if(!count)break;memcpy(line,p,count);line[count]=0;yy+=14*s;nc_center(x,yy,w,line,3,dim);p+=count;while(*p==' ')++p;}return;
    }
    if(n->kind==RISC_SCENE_ACTION){nc_control(x,y+4*s,w,h-8*s,n->label,!!(n->flags&RISC_SCENE_PRIMARY),HIT_ACTION,n,0);return;}
    if(n->kind==RISC_COMPONENT_SEGMENTS){
        unsigned count=(unsigned)(n->maximum-n->minimum+1);for(unsigned i=0;i<count;i++){char choice[32];nc_choice(d->choices,i,choice,sizeof(choice));int cw=w/(int)count;nc_control(x+(int)i*cw,y+4*s,cw-2*s,h-8*s,choice,n->value==(int)i+n->minimum,HIT_VALUE,n,(int)i+n->minimum);}return;
    }
    if(n->kind==RISC_COMPONENT_MARKERS){
        int cw=w/3;for(unsigned i=0;i<6;i++){int xx=x+(int)(i%3)*cw,yy=y+(int)(i/3)*44*s;bool on=n->value==(int)i;nc_box(xx,yy,cw-4*s,40*s,4*s,on?2*s:s,accent,on&&nc_paper()?fg:nc_bg());nc_marker(i,xx+cw/2-7*s,yy+13*s,14*s,on&&nc_paper());nc_hit(xx,yy,cw-4*s,40*s,HIT_VALUE,n,(int)i,false);}return;
    }
    if(n->kind==RISC_COMPONENT_TIME_PICKER){
        int value=model->wheel_node==(int)index?model->wheel_value:n->value;int values[]={value/60,value%60};
        for(unsigned i=0;i<2;i++){
            int xx=x+(int)i*w/2,cw=w/2;
            if(nc_paper()){
                nc_control(xx+12*s,y+2*s,cw-24*s,38*s,"+",false,HIT_VALUE,n,changed_value(n,1,i?1:60));
                nc_center(xx,y+62*s,cw,(char[4]){0},3,dim);
                char str[8];snprintf(str,sizeof(str),"%02d",(values[i]+1)%(i?60:24));nc_center(xx,y+68*s,cw,str,2,dim);
                snprintf(str,sizeof(str),"%02d",values[i]);nc_center(xx,y+103*s,cw,str,4,fg);
                snprintf(str,sizeof(str),"%02d",(values[i]+(i?59:23))%(i?60:24));nc_center(xx,y+132*s,cw,str,2,dim);
                nc_control(xx+12*s,y+144*s,cw-24*s,38*s,"-",false,HIT_VALUE,n,changed_value(n,-1,i?1:60));
            }else{
                for(int offset=-1;offset<=1;offset++){char str[8];snprintf(str,sizeof(str),"%02d",(values[i]+offset+(i?60:24))%(i?60:24));int shift=model->wheel_node==(int)index&&model->wheel_part==(int)i?model->wheel_offset:0;nc_center(xx,y+(54+offset*30)*s+shift,cw,str,offset?2:4,offset?dim:accent);}
                fill(xx+12*s,y+28*s,cw-24*s,s,accent);fill(xx+12*s,y+62*s,cw-24*s,s,accent);
                nc_hit(xx,y,cw,h,HIT_VALUE,n,changed_value(n,1,i?1:60),false);
            }
        }return;
    }
    if(n->kind==RISC_COMPONENT_STEPPER){
        nc_text(x,y+15*s,w,n->label,1,dim);int cy=y+24*s,ch=36*s;
        nc_control(x,cy,36*s,ch,"-",false,HIT_VALUE,n,changed_value(n,-1,n->step));
        nc_center(x+40*s,cy+23*s,w-80*s,n->text,1,fg);
        nc_control(x+w-36*s,cy,36*s,ch,"+",false,HIT_VALUE,n,changed_value(n,1,n->step));return;
    }
    if(n->kind==RISC_COMPONENT_SWITCH){
        nc_text(x,y+h/2+4*s,w-92*s,n->label,2,fg);
        nc_control(x+w-88*s,y+4*s,44*s,h-8*s,"OFF",!n->value,HIT_VALUE,n,0);
        nc_control(x+w-44*s,y+4*s,44*s,h-8*s,"ON",!!n->value,HIT_VALUE,n,1);return;
    }
    bool checked=n->kind==RISC_COMPONENT_CHECK_ROW;
    int left=x,right=x+w;
    if(checked){
        int bx=x+4*s,by=y+h/2-9*s;nc_box(bx,by,18*s,18*s,3*s,nc_paper()?2*s:s,n->value?nc_color(RISC_TONE_SUCCESS):accent,n->value?nc_color(RISC_TONE_SUCCESS):nc_bg());
        if(n->value)nc_icon(RISC_SYMBOL_CHECK,bx+2*s,by+2*s,12*s,nc_bg());
        nc_hit(x,y,34*s,h,HIT_VALUE,n,!n->value,false);left+=34*s;
        if(n->value)fg=dim;
    }else if(d->symbol){nc_icon(d->symbol,x+2*s,y+h/2-8*s,16*s,accent);left+=28*s;}
    else if(d->marker<6){nc_marker(d->marker,x+4*s,y+h/2-5*s,10*s,false);left+=24*s;}
    if(d->badge[0]){
        int bw=nc_measure(1,d->badge)+8*s;right-=bw+14*s;
        nc_box(right,y+5*s,bw,18*s,3*s,s,accent,nc_paper()&&d->tone==RISC_TONE_WARNING?fg:nc_bg());
        nc_center(right,y+17*s,bw,d->badge,1,nc_paper()&&d->tone==RISC_TONE_WARNING?nc_bg():accent);
    }else right-=12*s;
    nc_text(left,y+(n->text[0]?18*s:h/2+4*s),right-left,n->label,2,fg);
    if(n->text[0])nc_text(left,y+33*s,right-left,n->text,3,dim);
    if(checked&&n->value)fill(left,y+14*s,nc_measure(2,n->label)<right-left?nc_measure(2,n->label):right-left,s,dim);
    nc_line(x+w-6*s,y+h/2-3*s,x+w-3*s,y+h/2,s,dim);nc_line(x+w-3*s,y+h/2,x+w-6*s,y+h/2+3*s,s,dim);
    fill(x,y+h-1,w,1,nc_paper()?fg:0x1125);
    if(n->id==path.focus[path.depth-1])fill(x-6*s,y+5*s,2*s,h-10*s,accent);
    if(checked){if(d->secondary_action)nc_hit(left,y,x+w-left,h,HIT_SECONDARY,n,0,false);}
    else if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);
}
static void nc_header(int y){
    int s=nc_scale(),hh=nc_header_height(),left=16*s,right=(int)width-16*s;uint16_t accent=nc_color(RISC_TONE_ACCENT);
    fill(0,y,(int)width,hh,nc_bg());
    int r=route_index(&document,current_route());bool back_button=r>=0&&(document.routes[r].back_action||document.flags||path.depth>1);
    if(back_button){nc_box(left,y+8*s,28*s,28*s,14*s,s,accent,nc_bg());nc_icon(RISC_SYMBOL_BACK,left+7*s,y+15*s,14*s,accent);nc_hit(left-3*s,y+4*s,34*s,38*s,HIT_BACK,NULL,0,true);left+=38*s;}
    for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].kind==RISC_COMPONENT_HEADER_ACTION&&document.nodes[i].route==current_route()){
        const risc_scene_node_v1 *n=&document.nodes[i];if(n->flags&RISC_SCENE_HIDDEN)continue;right-=34*s;nc_box(right+3*s,y+8*s,28*s,28*s,14*s,s,accent,nc_bg());nc_icon(document.details[i].symbol,right+10*s,y+15*s,14*s,accent);nc_hit(right,y+4*s,34*s,38*s,HIT_ACTION,n,0,true);
    }
    if(r>=0)nc_text(left,y+27*s,right-left-4*s,document.routes[r].title,0,accent);
    if(document.subtitle[0])nc_text(16*s,y+48*s,(int)width-32*s,document.subtitle,3,nc_color(RISC_TONE_MUTED));
    fill(16*s,y+hh-s,(int)width-32*s,s,nc_paper()?nc_fg():0x1125);
}
static void component_draw(void){
    fill(0,0,(int)width,(int)height,nc_bg());
    upload=(hit_map){.revision=document.revision,.route=current_route(),.epoch=epoch,.keyboard=current_keyboard()!=NULL};
    const risc_scene_node_v1 *keyboard=current_keyboard();
    if(keyboard){if(nc_paper())paper_keyboard(keyboard);else {
        nc_header(0);int s=nc_scale();const char *tail=keyboard->text;while(*tail&&nc_measure(2,tail)>(int)width-32*s)++tail;nc_text(16*s,76*s,(int)width-32*s,tail,2,nc_fg());
        for(unsigned i=0;;i++){keyboard_key k;if(!keyboard_bounds((unsigned)keyboard->value,i,&k))break;char ch[2];const char *label=keyboard_caption((unsigned)keyboard->value,k.key,ch);
            bool selected=keyboard->id==path.focus[path.depth-1]&&i==focus_part;nc_box(k.x,k.y,k.w,k.h,3*s,s,nc_color(RISC_TONE_ACCENT),selected?nc_color(RISC_TONE_ACCENT):nc_bg());nc_center(k.x,k.y+k.h/2+4*s,k.w,label,3,selected?nc_bg():nc_fg());nc_hit(k.x,k.y,k.w,k.h,HIT_VALUE,keyboard,(int)k.key,true);
        }
    }return;}
    int hh=nc_header_height(),content=hh+nc_size(16);
    for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].route==current_route()&&!(document.nodes[i].flags&RISC_SCENE_HIDDEN))content+=nc_row_height(&document.nodes[i]);
    model->scroll_limit=content>(int)height?content-(int)height:0;
    if(model->scroll>model->scroll_limit)model->scroll=model->scroll_limit;
    if(model->scroll<0)model->scroll=0;
    int y=hh-model->scroll;
    if(!nc_paper())nc_header(-model->scroll);
    unsigned original_top=clip_top;
    if(nc_paper()&&clip_top<(unsigned)hh)clip_top=(unsigned)hh;
    for(unsigned i=0;i<document.node_count;i++){
        const risc_scene_node_v1 *n=&document.nodes[i];if(n->route!=current_route()||n->flags&RISC_SCENE_HIDDEN)continue;
        if(n->kind==RISC_COMPONENT_HEADER_ACTION)continue;
        nc_draw_node(i,y);y+=nc_row_height(n);
    }
    clip_top=original_top;if(nc_paper())nc_header(0);
    /* Edge scrim: a short ordered pattern remains binary on e-ink. */
    if(model->scroll<model->scroll_limit&&!layout_only)for(int yy=0;yy<8*nc_scale();yy++)for(int xx=0;xx<(int)width;xx++)if((xx+yy)%8<yy/nc_scale())pixel(xx,(int)height-8*nc_scale()+yy,nc_bg());
    if(document.flags&&document.cancel_action){
        nc_hit(0,0,nc_size(10),(int)height,HIT_BACK,NULL,0,true);nc_hit((int)width-nc_size(10),0,nc_size(10),(int)height,HIT_BACK,NULL,0,true);
        if(y<(int)height)nc_hit(0,y,(int)width,(int)height-y,HIT_BACK,NULL,0,true);
        if(!path.focus[path.depth-1])for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==document.cancel_action){path.focus[path.depth-1]=document.nodes[i].id;break;}
    }
    if(!document.flags&&model->toast_until&&document.toast[0]){
        int s=nc_scale(),ty=(int)height-42*s;uint16_t bg=nc_paper()?nc_fg():0x09c8,fg=nc_paper()?nc_bg():nc_fg();
        nc_round(12*s,ty,(int)width-24*s,34*s,6*s,bg);nc_text(20*s,ty+21*s,(int)width-(document.toast_action&&!nc_paper()?90:40)*s,document.toast,3,fg);
        if(document.toast_action&&!nc_paper()){nc_text((int)width-58*s,ty+21*s,48*s,"UNDO",1,fg);add_hit((int)width-70*s,ty,58*s,34*s,HIT_TOAST,0,0,0);}
    }
}
static bool component_activate(const hit *h){
    if(h->kind==HIT_TOAST){if(model->toast_until&&document.toast_action)emit(RISC_SCENE_ACTION_EVENT,0,document.toast_action,0);return true;}
    int i=node_index(&document,h->node);if(i<0)return true;const risc_scene_node_v1 *n=&document.nodes[i];
    if(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))return true;
    if(h->kind==HIT_SECONDARY){path.focus[path.depth-1]=n->id;emit(RISC_SCENE_ACTION_EVENT,n->id,document.details[i].secondary_action,0);return true;}return false;
}
static void component_down(int x,int y){
    model->drag_scroll=model->scroll;model->wheel_node=-1;model->wheel_offset=0;
    if(nc_paper())return;
    for(unsigned i=0;i<visible.count;i++){
        const hit *h=&visible.items[i];int at=node_index(&document,h->node);
        if(at>=0&&document.nodes[at].kind==RISC_COMPONENT_TIME_PICKER&&x>=h->x&&x<h->x+h->w&&y>=h->y&&y<h->y+h->h){model->wheel_node=at;model->wheel_part=x<(int)width/2?0:1;model->wheel_value=document.nodes[at].value;break;}
    }
}
static bool component_move(int x,int y,bool release){
    int dx=paper_abs(x-down_x),dy=y-down_y;if(!moved||dx>paper_abs(dy))return false;
    if(model->wheel_node>=0){
        const risc_scene_node_v1 *n=&document.nodes[model->wheel_node];int delta=down_y-y;if(release)delta+=delta>=0?nc_size(15):-nc_size(15);int step=delta/nc_size(30),v=n->value;
        model->wheel_offset=release?0:y-down_y+step*nc_size(30);
        if(model->wheel_part){int m=(v%60+step)%60;if(m<0)m+=60;v=v/60*60+m;}else{int h=(v/60+step)%24;if(h<0)h+=24;v=h*60+v%60;}
        model->wheel_value=v;dirty=true;
        if(release){emit(RISC_SCENE_VALUE_EVENT,n->id,n->action,v);model->wheel_node=-1;}
    }else {model->scroll=model->drag_scroll-dy;dirty=true;rebuild_hits();}
    return true;
}
static void component_focus(int direction){
    unsigned list[RISC_COMPONENTS_MAX_NODES],count=0;int current=-1;
    for(unsigned i=0;i<document.node_count;i++){const risc_scene_node_v1 *n=&document.nodes[i];if(n->route!=current_route()||n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)||!n->action)continue;if(n->id==path.focus[path.depth-1])current=(int)count;list[count++]=i;}
    if(!count)return;
    current=current<0?(direction>0?0:(int)count-1):(current+(int)count+direction)%(int)count;
    unsigned target=list[current];path.focus[path.depth-1]=document.nodes[target].id;focus_part=0;
    int y=nc_header_height();for(unsigned i=0;i<target;i++)if(document.nodes[i].route==current_route()&&!(document.nodes[i].flags&RISC_SCENE_HIDDEN))y+=nc_row_height(&document.nodes[i]);
    int min=nc_paper()?nc_header_height():0;if(y-model->scroll<min)model->scroll=y-min;
    if(y+nc_row_height(&document.nodes[target])-model->scroll>(int)height)model->scroll=y+nc_row_height(&document.nodes[target])-(int)height;
    invalidate_view();rebuild_hits();
}
static bool component_valid(const risc_components_document_v1 *d){
    if(!d->screen_key||d->flags&~3u||d->flags==3u||!terminated(d->subtitle,sizeof(d->subtitle))||!terminated(d->toast,sizeof(d->toast))||((d->flags!=0)!=(d->cancel_action!=0)))return false;
    bool cancel=!d->flags;unsigned headers=0;
    for(unsigned i=0;i<d->node_count;i++){
        const risc_scene_node_v1 *n=&d->nodes[i];const risc_component_detail_v1 *a=&d->details[i];
        if(n->kind==RISC_SCENE_TIME_OF_DAY||n->kind==RISC_SCENE_INTEGER||n->kind==RISC_SCENE_BOOLEAN||n->kind==RISC_SCENE_LINK)return false;
        if(a->symbol>RISC_SYMBOL_CHECK||a->marker>6||a->tone>RISC_TONE_MUTED||!terminated(a->badge,sizeof(a->badge))||!terminated(a->choices,sizeof(a->choices)))return false;
        if(n->kind==RISC_SCENE_ACTION&&n->action==d->cancel_action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_DESTRUCTIVE|RISC_SCENE_HIDDEN)))cancel=true;
        if(n->kind==RISC_COMPONENT_HEADER_ACTION&&++headers>2)return false;
        if(n->kind>=RISC_COMPONENT_ROW&&n->kind!=RISC_COMPONENT_PROGRESS&&n->kind!=RISC_COMPONENT_EMPTY&&!n->action)return false;
        if(n->kind==RISC_COMPONENT_CHECK_ROW||n->kind==RISC_COMPONENT_SWITCH){if(n->minimum||n->maximum!=1||n->step!=1||n->value<0||n->value>1)return false;}
        if(a->secondary_action&&n->kind!=RISC_COMPONENT_CHECK_ROW)return false;
        if(n->kind==RISC_COMPONENT_STEPPER||n->kind==RISC_COMPONENT_SEGMENTS||n->kind==RISC_COMPONENT_MARKERS||n->kind==RISC_COMPONENT_TIME_PICKER){
            if(n->minimum<0||n->maximum<n->minimum||n->maximum>65535||n->value<n->minimum||n->value>n->maximum||n->step!=1)return false;
        }
        if(n->kind==RISC_COMPONENT_TIME_PICKER&&(n->minimum||n->maximum!=1439))return false;
        if(n->kind==RISC_COMPONENT_MARKERS&&(n->minimum||n->maximum!=5))return false;
        if(n->kind==RISC_COMPONENT_SEGMENTS){unsigned count=1;bool empty=true;for(const char *p=a->choices;*p;p++){if(*p=='|'){if(empty)return false;count++;empty=true;}else empty=false;}if(empty||count>6||count!=(unsigned)(n->maximum-n->minimum+1))return false;}
        if(n->kind==RISC_COMPONENT_PROGRESS&&(n->value<0||n->maximum<0||n->maximum>65535||n->value>n->maximum))return false;
    }return cancel;
}
