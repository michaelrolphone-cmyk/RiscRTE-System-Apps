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
    static const pc_font *const faces[2][6]={{&nova_title_1,&nova_section_1,&nova_body_1,&nova_small_1,&nova_value_1,&nova_tab_1},{&nova_title_2,&nova_section_2,&nova_body_2,&nova_small_2,&nova_value_2,&nova_tab_2}};
    return faces[nc_scale()-1][role];
}
static int nc_tracking(unsigned role){return role==5?31*nc_scale():0;}
static int nc_measure(unsigned role,const char *s){return (paper_measure(nc_font(role),s,0)+(int)strlen(s)*nc_tracking(role)+32)/64;}
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
        pen+=g->advance+nc_tracking(role);
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
    case RISC_SYMBOL_PLACE:nc_line(x+a/2,y+m,x+m,y+a/2,st,c);nc_line(x+m,y+a/2,x+size-a/2,y+m,st,c);nc_line(x+a,y+m,x+a,y+size-a/2,st,c);nc_line(x+a,y+size-a/2,x+b,y+size-a/2,st,c);nc_line(x+b,y+size-a/2,x+b,y+m,st,c);break;
    case RISC_SYMBOL_EVENT:for(int i=0;i<4;i++){int z=i==1||i==2?size/2:size/4;nc_line(x+a/2+i*size/4,y+m-z/2,x+a/2+i*size/4,y+m+z/2,st,c);}break;
    case RISC_SYMBOL_TRIGGER:nc_box(x+a,y+a,size/2,size/2,m,st,c,nc_bg());nc_line(x+m,y,x+m,y+a/2,st,c);nc_line(x+m,y+b+a/2,x+m,y+size,st,c);nc_line(x,y+m,x+a/2,y+m,st,c);nc_line(x+b+a/2,y+m,x+size,y+m,st,c);break;
    case RISC_SYMBOL_LIBRARY:for(int i=0;i<3;i++)nc_box(x+a/2+i*size/3,y+a/2,size/4,size-a,st,st,c,nc_bg());break;
    case RISC_SYMBOL_SETTINGS:for(int i=0;i<3;i++){int yy=y+a+i*a;nc_line(x+a/2,yy,x+size-a/2,yy,st,c);nc_box(x+(i==1?b:a)-st,yy-st,3*st,3*st,st,st,c,nc_bg());}break;
    case RISC_SYMBOL_WORKFLOW:nc_box(x+a/2,y+a/2,size-a,size-a,st,st,c,nc_bg());nc_line(x+a,y+m,x+b,y+m,st,c);nc_line(x+m,y+a,x+b,y+m,st,c);nc_line(x+b,y+m,x+m,y+b,st,c);break;
    case RISC_SYMBOL_ACTIVITY:nc_line(x,y+m,x+a,y+m,st,c);nc_line(x+a,y+m,x+m,y+a,st,c);nc_line(x+m,y+a,x+b,y+b,st,c);nc_line(x+b,y+b,x+size,y+m,st,c);break;
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
static bool nc_toolbar(void){
    for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].route==current_route()&&document.nodes[i].kind==RISC_COMPONENT_HEADER_ACTION)return true;
    return false;
}
static bool nc_back_button(void){int r=route_index(&document,current_route());return !nc_toolbar()&&r>=0&&document.routes[r].back_action;}
static int nc_header_height(void){return nc_size(nc_toolbar()?(nc_paper()?56:78):document.subtitle[0]?(nc_paper()?60:84):(nc_paper()?44:68));}
static int nc_row_height(const risc_scene_node_v1 *n){
    switch(n->kind){
    case RISC_COMPONENT_HEADER_ACTION:return 0;
    case RISC_COMPONENT_SECTION:return nc_size(28);
    case RISC_COMPONENT_PROGRESS:return nc_size(n->label[0]?88:46);
    case RISC_COMPONENT_EMPTY:return nc_size(140);
    case RISC_COMPONENT_SEGMENTS:return nc_size(38);
    case RISC_COMPONENT_TIME_PICKER:return nc_size(nc_paper()?160:108);
    case RISC_COMPONENT_MARKERS:return nc_size(50);
    case RISC_COMPONENT_CARD:return nc_size(72);
    case RISC_COMPONENT_RING:return nc_size(nc_paper()?116:108);
    case RISC_SCENE_ACTION:return nc_size(50);
    case RISC_SCENE_TEXT_NODE:return nc_size(56);
    default:return nc_size(nc_paper()?42:40);
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
    nc_box(x,y,w,h,h/2,nc_size(nc_paper()?2:1),selected?ink:nc_paper()?ink:0x0a6b,selected?ink:bg);
    nc_center(x,y+h/2+nc_size(4),w,label,1,selected?bg:fg);
    if(n&&n->id==path.focus[path.depth-1])fill(x,y+h-nc_size(3),w,nc_size(3),ink);
    nc_hit(x,y,w,h,kind,n,value,false);
}
static void nc_choice(const char *all,unsigned index,char *out,size_t size){
    while(index&&*all){if(*all++=='|')--index;}size_t i=0;
    while(*all&&*all!='|'&&i+1<size)out[i++]=*all++;
    out[i]=0;
}
/* The same flow layout drives rendering, scrolling and keyboard focus. */
static int nc_layout(unsigned target,int *ox,int *oy,int *ow,int *oh){
    int s=nc_scale(),x=16*s,y=nc_header_height(),w=(int)width-32*s;bool chips=false;
    for(unsigned i=0;i<document.node_count;i++){
        const risc_scene_node_v1 *n=&document.nodes[i];
        if(n->route!=current_route()||n->flags&RISC_SCENE_HIDDEN||n->kind==RISC_COMPONENT_HEADER_ACTION)continue;
        int cw=w,ch=nc_row_height(n);
        if(n->kind==RISC_COMPONENT_CHIP){
            if(!chips){y+=8*s;x=16*s;chips=true;}
            cw=nc_measure(1,n->label)+24*s;if(cw>w)cw=w;
            if(x+cw>(int)width-16*s){x=16*s;y+=36*s;}
            ch=30*s;
        }else if(chips){y+=38*s;x=16*s;chips=false;}
        if(i==target){*ox=x;*oy=y;*ow=cw;*oh=ch;return y+ch;}
        if(chips)x+=cw+6*s;else y+=ch;
    }
    return y+(chips?38*s:0);
}
static void nc_draw_node(unsigned index,int x,int y,int w,int h){
    const risc_scene_node_v1 *n=&document.nodes[index];const risc_component_detail_v1 *d=&document.details[index];
    int s=nc_scale();
    if(y+h<0||y>(int)height)return;
    uint16_t fg=nc_fg(),dim=nc_color(RISC_TONE_MUTED),accent=nc_color(d->tone?d->tone:RISC_TONE_ACCENT);
    if(n->flags&RISC_SCENE_DISABLED){fg=dim;accent=dim;}
    if(n->kind==RISC_COMPONENT_SECTION){nc_text(x,y+20*s,w,n->label,1,dim);return;}
    if(n->kind==RISC_COMPONENT_CARD){
        bool inverse=nc_paper()&&(n->flags&RISC_SCENE_PRIMARY);uint16_t cardfg=inverse?nc_bg():accent;
        nc_box(x,y+8*s,w,h-16*s,12*s,nc_paper()?5:s,nc_paper()?fg:0x0a6b,inverse?fg:nc_bg());
        nc_text(x+11*s,y+30*s,w-22*s,n->label,0,cardfg);nc_text(x+11*s,y+47*s,w-22*s,n->text,3,inverse?nc_bg():dim);
        if(n->action)nc_hit(x,y+8*s,w,h-16*s,HIT_ACTION,n,0,false);
        return;
    }
    if(n->kind==RISC_COMPONENT_RING){
        int cx=(int)width/2,cy=y+h/2,r=44*s,stroke=(nc_paper()?9:7)*s;
        /* Integer octants avoid a floating-point dependency in the presenter. */
        for(int yy=-r;yy<=r;yy++)for(int xx=-r;xx<=r;xx++){
            int rr=xx*xx+yy*yy;if(rr>r*r||rr<(r-stroke)*(r-stroke))continue;
            int ax=paper_abs(xx),ay=paper_abs(yy),q=ax+ay?ax*25/(ax+ay):0;
            int percent=yy<=0?(xx>=0?q:100-q):(xx>=0?50-q:50+q);
            pixel(cx+xx,cy+yy,percent* n->maximum<=n->value*100?accent:nc_paper()?fg:0x1125);
        }
        nc_center(cx-35*s,cy+9*s,70*s,d->badge,4,fg);return;
    }
    if(n->kind==RISC_COMPONENT_STRENGTH){
        nc_text(x+10*s,y+h/2+4*s,w-100*s,n->label,2,fg);
        for(int i=0;i<3;i++)nc_box(x+w-(96-i*15)*s,y+h/2-3*s,12*s,6*s,3*s,s,accent,i<n->value?accent:nc_bg());
        nc_text(x+w-45*s,y+h/2+4*s,45*s,n->text,3,accent);
        fill(x,y,(nc_paper()?3:2)*s,h,nc_paper()?fg:0x1125);fill(x,y+h-s,w,s,nc_paper()?fg:0x1125);return;
    }
    if(n->kind==RISC_COMPONENT_PROGRESS){
        if(n->label[0]){nc_box(x,y+8*s,w,h-16*s,12*s,nc_paper()?5:s,nc_paper()?fg:0x0a6b,nc_bg());nc_text(x+11*s,y+30*s,w-22*s,n->label,0,accent);nc_text(x+11*s,y+44*s,w-22*s,n->text,3,dim);}
        else nc_center(x,y+15*s,w,n->text,3,dim);
        int bx=x+(n->label[0]?27*s:0),bw=w-(n->label[0]?54*s:0),by=y+h-(n->label[0]?36:20)*s;
        nc_box(bx,by,bw,12*s,6*s,2*s,accent,nc_bg());
        int filled=n->maximum>0?(bw*n->value/n->maximum):0;
        if(filled>0)nc_round(bx,by,filled,12*s,6*s,accent);
        if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);
        return;
    }
    if(n->kind==RISC_COMPONENT_EMPTY||n->kind==RISC_SCENE_TEXT_NODE){
        int yy=y+(n->kind==RISC_COMPONENT_EMPTY?88:18)*s;
        if(n->kind==RISC_COMPONENT_EMPTY){
            /* Bible empty state: 44 logical px circle, 20 px check, dim ink.
             * Paper uses a 2.5 logical px outline (5 physical pixels). */
            int cx=(int)width/2,cy=y+44*s;
            if(!d->symbol||d->symbol==RISC_SYMBOL_CHECK){
                nc_box(cx-22*s,cy-22*s,44*s,44*s,22*s,nc_paper()?5:s,nc_paper()?fg:0x0a6b,nc_bg());
                nc_icon(RISC_SYMBOL_CHECK,cx-12*s,cy-10*s,20*s,dim);
            }else nc_icon(d->symbol,cx-17*s,cy-17*s,34*s,accent);
        }
        nc_center(x,yy,w,n->label,1,fg);
        /* Wrap explanatory content on spaces, with body-size glyphs. */
        const char *p=n->text;while(*p&&yy<y+h-8*s){char line[72];unsigned count=0,space=0;while(p[count]&&count<70){line[count]=p[count];line[count+1]=0;if(nc_measure(3,line)>w-8*s)break;if(p[count]==' ')space=count;++count;}if(p[count]&&space)count=space;if(!count)break;memcpy(line,p,count);line[count]=0;yy+=14*s;nc_center(x,yy,w,line,3,dim);p+=count;while(*p==' ')++p;}return;
    }
    if(n->kind==RISC_SCENE_ACTION){nc_control(x,y+8*s,w,34*s,n->label,!!(n->flags&RISC_SCENE_PRIMARY),HIT_ACTION,n,0);return;}
    if(n->kind==RISC_COMPONENT_CHIP){nc_control(x,y,w,h,n->label,false,HIT_ACTION,n,0);return;}
    if(n->kind==RISC_COMPONENT_SEGMENTS){
        /* One capsule with an inset selection, not a row of outlined buttons.
         * Bible: 26 high, 2 inset, 3 gap, 22 high tabs, 8px Orbitron labels. */
        unsigned count=(unsigned)(n->maximum-n->minimum+1);int available=w-4*s-((int)count-1)*3*s;
        nc_box(x,y+8*s,w,26*s,12*s,nc_paper()?2*s:s,nc_paper()?fg:0x0a6b,nc_bg());
        for(unsigned i=0;i<count;i++){
            char choice[32];nc_choice(d->choices,i,choice,sizeof(choice));
            int left=available*(int)i/(int)count,right=available*(int)(i+1)/(int)count;
            int xx=x+2*s+left+(int)i*3*s,cw=right-left,value=(int)i+n->minimum;
            bool on=n->value==value;
            if(on)nc_round(xx,y+10*s,cw,22*s,9*s,accent);
            nc_center(xx,y+24*s,cw,choice,5,on?nc_bg():dim);
            if(n->id==path.focus[path.depth-1]&&focus_part==i&&!on)fill(xx+8*s,y+29*s,cw-16*s,s,accent);
            nc_hit(xx,y+10*s,cw,22*s,HIT_VALUE,n,value,false);
        }return;
    }
    if(n->kind==RISC_COMPONENT_MARKERS){
        int available=w-30*s;for(unsigned i=0;i<6;i++){int left=available*(int)i/6,right=available*(int)(i+1)/6,xx=x+left+(int)i*6*s,cw=right-left,yy=y+8*s;bool on=n->value==(int)i;nc_box(xx,yy,cw,34*s,9*s,on?3*s:nc_paper()?2*s:s,accent,on&&nc_paper()?fg:nc_bg());nc_marker(i,xx+cw/2-5*s,yy+12*s,10*s,on&&nc_paper());nc_hit(xx,yy,cw,34*s,HIT_VALUE,n,(int)i,false);}return;
    }
    if(n->kind==RISC_COMPONENT_TIME_PICKER){
        int value=model->wheel_node==(int)index?model->wheel_value:n->value;int values[]={value/60,value%60};
        for(unsigned i=0;i<2;i++){
            int xx=x+(int)i*w/2,cw=w/2;
            if(nc_paper()){
                int px=xx+(i?5:0)*s,pw=cw-5*s;
                nc_box(px,y+6*s,pw,30*s,9*s,5,fg,nc_bg());nc_center(px,y+25*s,pw,"+",1,fg);
                nc_hit(px,y+6*s,pw,30*s,HIT_VALUE,n,changed_value(n,1,i?5:60),false);
                char str[8];snprintf(str,sizeof(str),"%02d",(values[i]+(i?5:1))%(i?60:24));nc_center(px,y+50*s,pw,str,1,dim);
                nc_round(px,y+56*s,pw,46*s,9*s,fg);
                snprintf(str,sizeof(str),"%02d",values[i]);nc_center(px,y+90*s,pw,str,4,nc_bg());
                snprintf(str,sizeof(str),"%02d",(values[i]+(i?55:23))%(i?60:24));nc_center(px,y+119*s,pw,str,1,dim);
                nc_box(px,y+124*s,pw,30*s,9*s,5,fg,nc_bg());nc_center(px,y+143*s,pw,"-",1,fg);
                nc_hit(px,y+124*s,pw,30*s,HIT_VALUE,n,changed_value(n,-1,i?5:60),false);
            }else{
                for(int offset=-1;offset<=1;offset++){char str[8];snprintf(str,sizeof(str),"%02d",(values[i]+offset+(i?60:24))%(i?60:24));int shift=model->wheel_node==(int)index&&model->wheel_part==(int)i?model->wheel_offset:0;nc_center(xx,y+(64+offset*30)*s+shift,cw,str,offset?2:4,offset?dim:accent);}
                fill(xx+12*s,y+38*s,cw-24*s,s,accent);fill(xx+12*s,y+70*s,cw-24*s,s,accent);
                nc_hit(xx,y,cw,h,HIT_VALUE,n,changed_value(n,1,i?1:60),false);
            }
        }if(!nc_paper())nc_center((int)width/2-8*s,y+64*s,16*s,":",4,accent);return;
    }
    if(n->kind==RISC_COMPONENT_STEPPER){
        int bx=x+w-114*s,cy=y+h/2-11*s;
        nc_text(x+8*s,y+h/2+4*s,bx-x-12*s,n->label,2,fg);
        nc_control(bx,cy,22*s,22*s,"-",false,HIT_VALUE,n,changed_value(n,-1,n->step));
        nc_center(bx+25*s,cy+14*s,64*s,n->text,1,fg);
        nc_control(x+w-22*s,cy,22*s,22*s,"+",false,HIT_VALUE,n,changed_value(n,1,n->step));
        fill(x,y,nc_paper()?3*s:2*s,h,nc_paper()?fg:0x1125);fill(x,y+h-s,w,s,nc_paper()?fg:0x1125);return;
    }
    if(n->kind==RISC_COMPONENT_SWITCH){
        nc_text(x+8*s,y+h/2+4*s,w-76*s,n->label,2,fg);int xx=x+w-60*s,yy=y+h/2-11*s;
        nc_box(xx,yy,60*s,22*s,10*s,nc_paper()?2*s:s,nc_paper()?fg:0x0a6b,nc_bg());
        for(int i=0;i<2;i++){bool on=n->value==i;uint16_t ink=i?accent:nc_paper()?fg:0x1125;
            if(on)nc_round(xx+(2+i*28)*s,yy+2*s,28*s,18*s,8*s,ink);
            nc_center(xx+(2+i*28)*s,yy+14*s,28*s,i?"ON":"OFF",5,on?(nc_paper()||i?nc_bg():fg):dim);
            nc_hit(xx+(2+i*28)*s,yy+2*s,28*s,18*s,HIT_VALUE,n,i,false);
        }fill(x,y,nc_paper()?3*s:2*s,h,nc_paper()?fg:0x1125);fill(x,y+h-s,w,s,nc_paper()?fg:0x1125);return;
    }
    bool checked=n->kind==RISC_COMPONENT_CHECK_ROW;
    /* Row padding begins inside the profile's frame, as in the NOVA-7 rows. */
    int left=x+(nc_paper()?3:2)*s+8*s,right=x+w;
    if(checked){
        accent=n->flags&RISC_SCENE_DISABLED?dim:nc_color(RISC_TONE_ACCENT);
        int bx=d->secondary_action?left:x+w-22*s,by=y+h/2-10*s;nc_box(bx,by,20*s,20*s,6*s,2*s,accent,n->value?accent:nc_bg());
        if(n->value)nc_icon(RISC_SYMBOL_CHECK,bx+2*s,by+2*s,12*s,nc_bg());
        if(!d->secondary_action){nc_text(left,y+h/2+4*s,w-38*s,n->label,2,fg);nc_hit(x,y,w,h,HIT_VALUE,n,!n->value,false);fill(x,y,2*s,h,nc_paper()?fg:0x1125);fill(x,y+h-s,w,s,nc_paper()?fg:0x1125);return;}
        nc_hit(x,y,34*s,h,HIT_VALUE,n,!n->value,false);left+=34*s;
        if(n->value)fg=dim;
    }else if(d->symbol){nc_icon(d->symbol,left,y+h/2-8*s,16*s,accent);left+=(16+7)*s;}
    else if(d->marker<6){nc_marker(d->marker,left+2*s,y+h/2-5*s,10*s,false);left+=(14+7)*s;}
    if(d->badge[0]&&n->kind==RISC_COMPONENT_STAT_ROW){
        int bw=nc_measure(2,d->badge);right-=bw+12*s;nc_text(right,y+h/2+4*s,bw,d->badge,2,accent);right-=7*s;
    }else if(d->badge[0]){
        int bw=nc_measure(5,d->badge)+12*s;right-=bw+14*s;
        nc_box(right,y+5*s,bw,18*s,8*s,s,accent,d->tone==RISC_TONE_WARNING?accent:nc_bg());
        nc_center(right,y+17*s,bw,d->badge,5,d->tone==RISC_TONE_WARNING?nc_bg():accent);
    }else right-=12*s;
    nc_text(left,y+(n->text[0]?18*s:h/2+4*s),right-left,n->label,2,fg);
    if(n->text[0])nc_text(left,y+33*s,right-left,n->text,3,dim);
    if(checked&&n->value)fill(left,y+14*s,nc_measure(2,n->label)<right-left?nc_measure(2,n->label):right-left,s,dim);
    if(n->action){nc_line(x+w-6*s,y+h/2-3*s,x+w-3*s,y+h/2,s,dim);nc_line(x+w-3*s,y+h/2,x+w-6*s,y+h/2+3*s,s,dim);}
    fill(x,y+h-1,w,1,nc_paper()?fg:0x1125);
    fill(x,y,nc_paper()?3*s:2*s,h,nc_paper()?fg:0x1125);
    if(n->id==path.focus[path.depth-1])fill(x-6*s,y+5*s,2*s,h-10*s,accent);
    if(checked){if(d->secondary_action)nc_hit(left,y,x+w-left,h,HIT_SECONDARY,n,0,false);}
    else if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);
}
static void nc_header(int y){
    int s=nc_scale(),hh=nc_header_height(),left=16*s,right=(int)width-16*s;
    uint16_t accent=nc_color(RISC_TONE_ACCENT),line=nc_paper()?nc_fg():0x0a6b;
    fill(0,y,(int)width,hh,nc_bg());int r=route_index(&document,current_route());
    if(nc_toolbar()){
        int top=y+(nc_paper()?18:42)*s;
        if(r>=0&&document.routes[r].back_action){nc_box(left,top,28*s,28*s,14*s,nc_paper()?5:s,line,nc_bg());nc_icon(RISC_SYMBOL_BACK,left+7*s,top+7*s,14*s,accent);nc_hit(left-3*s,top-3*s,34*s,34*s,HIT_BACK,NULL,0,true);left+=35*s;}
        for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].kind==RISC_COMPONENT_HEADER_ACTION&&document.nodes[i].route==current_route()){
            const risc_scene_node_v1 *n=&document.nodes[i];if(n->flags&RISC_SCENE_HIDDEN)continue;right-=35*s;
            nc_box(right+7*s,top,28*s,28*s,14*s,nc_paper()?5:s,line,nc_bg());nc_icon(document.details[i].symbol,right+14*s,top+7*s,14*s,accent);nc_hit(right+4*s,top-3*s,34*s,34*s,HIT_ACTION,n,0,true);
        }
        if(r>=0)nc_text(left,top+18*s,right-left+3*s,document.routes[r].title,0,accent);
    }else{
        int top=y+(nc_paper()?20:44)*s;
        if(r>=0)nc_text(left,top+13*s,right-left,document.routes[r].title,0,accent);
        if(document.subtitle[0])nc_text(left,top+27*s,right-left,document.subtitle,3,nc_color(RISC_TONE_MUTED));
    }
    fill(16*s,y+hh-(nc_paper()?2*s:s),(int)width-32*s,nc_paper()?2*s:s,line);
}
static void nc_modal(void){
    int s=nc_scale(),w=(document.flags==RISC_COMPONENTS_ALERT?206:196)*s,h=140*s,x=((int)width-w)/2,y=((int)height-h)/2;
    uint16_t ink=nc_color(RISC_TONE_ACCENT);unsigned actions=0,body=0;
    for(unsigned i=0;i<document.node_count;i++)if(!(document.nodes[i].flags&RISC_SCENE_HIDDEN)){
        if(document.nodes[i].kind==RISC_SCENE_ACTION)actions++;
        if(document.nodes[i].kind==RISC_SCENE_TEXT_NODE)body=i;
    }
    nc_box(x,y,w,h,16*s,nc_paper()?3*s:s,nc_paper()?nc_fg():0x0a6b,nc_bg());
    int r=route_index(&document,current_route());if(r>=0)nc_center(x+12*s,y+23*s,w-24*s,document.routes[r].title,5,ink);
    const risc_scene_node_v1 *text=&document.nodes[body];
    nc_center(x+12*s,y+47*s,w-24*s,text->label,2,nc_fg());
    const char *p=text->text;int yy=y+64*s;
    while(*p&&yy<y+h-46*s){
        char line[RISC_SCENE_TEXT];unsigned count=0,space=0;
        while(p[count]&&count<sizeof(line)-1){line[count]=p[count];line[count+1]=0;if(nc_measure(3,line)>w-24*s)break;if(p[count]==' ')space=count;++count;}
        if(p[count]&&space)count=space;
        if(!count)break;
        memcpy(line,p,count);line[count]=0;
        nc_center(x+12*s,yy,w-24*s,line,3,nc_color(RISC_TONE_MUTED));yy+=13*s;p+=count;while(*p==' ')++p;
    }
    unsigned at=0;int available=w-24*s-((int)actions-1)*6*s;
    for(unsigned i=0;i<document.node_count;i++){
        const risc_scene_node_v1 *n=&document.nodes[i];if(n->kind!=RISC_SCENE_ACTION||n->flags&RISC_SCENE_HIDDEN)continue;
        int left=available*(int)at/(int)actions,right=available*(int)(at+1)/(int)actions,xx=x+12*s+left+(int)at*6*s,cw=right-left,by=y+h-40*s;
        bool selected=n->flags&RISC_SCENE_PRIMARY;uint16_t fg=n->flags&RISC_SCENE_DESTRUCTIVE?nc_color(RISC_TONE_DANGER):ink;
        nc_box(xx,by,cw,28*s,14*s,nc_paper()?2*s:s,nc_paper()?fg:0x0a6b,selected?ink:nc_bg());
        nc_center(xx,by+17*s,cw,n->label,5,selected?nc_bg():fg);
        if(!path.focus[path.depth-1]&&n->action==document.cancel_action)path.focus[path.depth-1]=n->id;
        nc_hit(xx,by,cw,28*s,HIT_ACTION,n,0,true);++at;
    }
    /* Only the exterior cancels. Empty dialog interior is inert. */
    nc_hit(0,0,(int)width,y,HIT_BACK,NULL,0,true);nc_hit(0,y+h,(int)width,(int)height-y-h,HIT_BACK,NULL,0,true);
    nc_hit(0,y,x,h,HIT_BACK,NULL,0,true);nc_hit(x+w,y,(int)width-x-w,h,HIT_BACK,NULL,0,true);
    model->scroll=model->scroll_limit=0;
}
static void component_draw(void){
    fill(0,0,(int)width,(int)height,nc_bg());
    upload=(hit_map){.revision=document.revision,.route=current_route(),.epoch=epoch,.keyboard=current_keyboard()!=NULL};
    if(document.flags){nc_modal();return;}
    const risc_scene_node_v1 *keyboard=current_keyboard();
    if(keyboard){if(nc_paper())paper_keyboard(keyboard);else {
        int s=nc_scale();nc_text(54*s,28*s,(int)width-70*s,keyboard->label,0,nc_color(RISC_TONE_ACCENT));
        nc_box(16*s,10*s,28*s,28*s,14*s,s,nc_color(RISC_TONE_ACCENT),nc_bg());
        nc_icon(RISC_SYMBOL_BACK,23*s,17*s,14*s,nc_color(RISC_TONE_ACCENT));
        nc_hit(13*s,7*s,34*s,34*s,HIT_VALUE,keyboard,RISC_SCENE_KEY_CANCEL,true);
        nc_box(16*s,42*s,(int)width-32*s,32*s,10*s,s,nc_color(RISC_TONE_ACCENT),nc_bg());
        const char *tail=keyboard->text;while(*tail&&nc_measure(2,tail)>(int)width-48*s)++tail;nc_text(24*s,63*s,(int)width-48*s,tail,2,nc_fg());
        for(unsigned i=0;;i++){keyboard_key k;if(!keyboard_bounds((unsigned)keyboard->value,i,&k))break;char ch[2];const char *label=keyboard_caption((unsigned)keyboard->value,k.key,ch);
            bool selected=keyboard->id==path.focus[path.depth-1]&&i==focus_part;nc_box(k.x,k.y,k.w,k.h,3*s,s,nc_color(RISC_TONE_ACCENT),selected?nc_color(RISC_TONE_ACCENT):nc_bg());nc_center(k.x,k.y+k.h/2+4*s,k.w,label,3,selected?nc_bg():nc_fg());nc_hit(k.x,k.y,k.w,k.h,HIT_VALUE,keyboard,(int)k.key,true);
        }
    }return;}
    int hh=nc_header_height(),x,y,w,h,content=nc_layout(document.node_count,&x,&y,&w,&h)+nc_size(nc_back_button()?64:30);
    model->scroll_limit=content>(int)height?content-(int)height:0;
    if(model->scroll>model->scroll_limit)model->scroll=model->scroll_limit;
    if(model->scroll<0)model->scroll=0;
    if(!nc_paper())nc_header(-model->scroll);
    unsigned original_top=clip_top;
    if(nc_paper()&&clip_top<(unsigned)hh)clip_top=(unsigned)hh;
    for(unsigned i=0;i<document.node_count;i++){
        const risc_scene_node_v1 *n=&document.nodes[i];if(n->route!=current_route()||n->flags&RISC_SCENE_HIDDEN)continue;
        if(n->kind==RISC_COMPONENT_HEADER_ACTION)continue;
        nc_layout(i,&x,&y,&w,&h);nc_draw_node(i,x,y-model->scroll,w,h);
    }
    clip_top=original_top;if(nc_paper())nc_header(0);
    /* Edge scrim: a short ordered pattern remains binary on e-ink. */
    if(model->scroll<model->scroll_limit&&!layout_only)for(int yy=0;yy<8*nc_scale();yy++)for(int xx=0;xx<(int)width;xx++)if((xx+yy)%8<yy/nc_scale())pixel(xx,(int)height-8*nc_scale()+yy,nc_bg());
    if(nc_back_button()){
        int s=nc_scale(),bx=((int)width-76*s)/2,by=(int)height-(nc_paper()?38:40)*s;
        nc_box(bx,by,76*s,26*s,13*s,nc_paper()?5:s,nc_paper()?nc_fg():0x0a6b,nc_bg());
        nc_center(bx,by+17*s,76*s,"< BACK",1,nc_color(RISC_TONE_ACCENT));nc_hit(bx,by,76*s,26*s,HIT_BACK,NULL,0,true);
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
    if(h->kind==HIT_VALUE&&n->kind==RISC_COMPONENT_SEGMENTS)focus_part=(unsigned)(h->value-n->minimum);
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
    int x,y,w,h;nc_layout(target,&x,&y,&w,&h);
    int min=nc_paper()?nc_header_height():0;if(y-model->scroll<min)model->scroll=y-min;
    if(y+h-model->scroll>(int)height)model->scroll=y+h-(int)height;
    invalidate_view();rebuild_hits();
}
static bool component_valid(const risc_components_document_v1 *d){
    if(!d->screen_key||d->flags&~3u||d->flags==3u||!terminated(d->subtitle,sizeof(d->subtitle))||!terminated(d->toast,sizeof(d->toast))||((d->flags!=0)!=(d->cancel_action!=0)))return false;
    bool cancel=!d->flags;unsigned headers=0,modal_actions=0,modal_texts=0;
    for(unsigned i=0;i<d->node_count;i++){
        const risc_scene_node_v1 *n=&d->nodes[i];const risc_component_detail_v1 *a=&d->details[i];
        if(d->flags){if(n->kind==RISC_SCENE_ACTION)modal_actions++;else if(n->kind==RISC_SCENE_TEXT_NODE)modal_texts++;else return false;}
        if(n->kind==RISC_SCENE_TIME_OF_DAY||n->kind==RISC_SCENE_INTEGER||n->kind==RISC_SCENE_BOOLEAN||n->kind==RISC_SCENE_LINK)return false;
        if(a->symbol>RISC_SYMBOL_ACTIVITY||a->marker>6||a->tone>RISC_TONE_MUTED||!terminated(a->badge,sizeof(a->badge))||!terminated(a->choices,sizeof(a->choices)))return false;
        if(n->kind==RISC_SCENE_ACTION&&n->action==d->cancel_action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_DESTRUCTIVE|RISC_SCENE_HIDDEN)))cancel=true;
        if(n->kind==RISC_COMPONENT_HEADER_ACTION&&++headers>2)return false;
        if(n->kind>=RISC_COMPONENT_ROW&&n->kind!=RISC_COMPONENT_PROGRESS&&n->kind!=RISC_COMPONENT_EMPTY&&n->kind<RISC_COMPONENT_CARD&&!n->action)return false;
        if(n->kind==RISC_COMPONENT_RING&&(n->maximum<1||n->maximum>100||n->value<0||n->value>n->maximum))return false;
        if(n->kind==RISC_COMPONENT_STRENGTH&&(n->value<0||n->value>3))return false;
        if(n->kind==RISC_COMPONENT_CHECK_ROW||n->kind==RISC_COMPONENT_SWITCH){if(n->minimum||n->maximum!=1||n->step!=1||n->value<0||n->value>1)return false;}
        if(a->secondary_action&&n->kind!=RISC_COMPONENT_CHECK_ROW)return false;
        if(n->kind==RISC_COMPONENT_STEPPER||n->kind==RISC_COMPONENT_SEGMENTS||n->kind==RISC_COMPONENT_MARKERS||n->kind==RISC_COMPONENT_TIME_PICKER){
            if(n->minimum<0||n->maximum<n->minimum||n->maximum>65535||n->value<n->minimum||n->value>n->maximum||n->step!=1)return false;
        }
        if(n->kind==RISC_COMPONENT_TIME_PICKER&&(n->minimum||n->maximum!=1439))return false;
        if(n->kind==RISC_COMPONENT_MARKERS&&(n->minimum||n->maximum!=5))return false;
        if(n->kind==RISC_COMPONENT_SEGMENTS){unsigned count=1;bool empty=true;for(const char *p=a->choices;*p;p++){if(*p=='|'){if(empty)return false;count++;empty=true;}else empty=false;}if(empty||count>6||count!=(unsigned)(n->maximum-n->minimum+1))return false;}
        if(n->kind==RISC_COMPONENT_PROGRESS&&(n->value<0||n->maximum<0||n->maximum>65535||n->value>n->maximum))return false;
    }return cancel&&(!d->flags||(modal_actions>=1&&modal_actions<=3&&modal_texts==1));
}
