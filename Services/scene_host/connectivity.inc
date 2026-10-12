/* Shared connectivity vocabulary. No radio calls, SSID policy or credentials. */
static void cn_signal(int x,int y,int h,unsigned bars,uint16_t fg,uint16_t bg){
    int s=nc_scale(),w=h*18/14;
    for(unsigned i=0;i<4;i++){
        int bh=h*(4+3*(int)i)/13,bw=w/5;
        nc_box(x+(int)i*w/4,y+h-bh,bw,bh,s,s,fg,i<bars?fg:bg);
    }
}
static void cn_badge(int x,int y,int w,const char *label,uint16_t fg,uint16_t bg){
    int s=nc_scale();nc_box(x,y,w,14*s,6*s,s,fg,bg);nc_center(x,y+10*s,w,label,5,fg);
}
static bool cn_draw(unsigned index,int x,int y,int w,int h){
    const risc_scene_node_v1 *n=&document.nodes[index];
    if(n->kind<RISC_COMPONENT_NETWORK_ROW||n->kind>RISC_COMPONENT_CONNECTION_STEP)return false;
    const risc_component_detail_v1 *d=&document.details[index];int s=nc_scale();
    bool inverse=(n->flags&RISC_SCENE_PRIMARY)!=0;
    uint16_t bg=inverse?nc_fg():nc_bg(),fg=inverse?nc_bg():nc_fg();
    if(n->kind==RISC_COMPONENT_NETWORK_ROW){
        if(inverse){fill(x,y,w,h,bg);}
        fill(x,y,2*s,h,fg);fill(x,y+h-2*s,w,2*s,fg);
        nc_icon(d->symbol,x+8*s,y+14*s,12*s,fg);
        nc_text(x+28*s,y+18*s,w-68*s,n->label,2,fg);
        int bx=x+28*s;
        if(d->badge[0]){int bw=nc_measure(5,d->badge)+10*s;cn_badge(bx,y+25*s,bw,d->badge,fg,bg);bx+=bw+4*s;}
        int bw=nc_measure(5,n->text)+10*s;if(bw>w-62*s-(bx-x))bw=w-62*s-(bx-x);
        if(bw>10*s)cn_badge(bx,y+25*s,bw,n->text,fg,bg);
        cn_signal(x+w-34*s,y+15*s,14*s,(unsigned)n->value,fg,bg);
        nc_text(x+w-8*s,y+27*s,8*s,">",3,fg);
        if(n->action)nc_hit(x,y,w,h,HIT_ACTION,n,0,false);
    }else if(n->kind==RISC_COMPONENT_CONNECTION_CARD){
        nc_box(x,y+8*s,w,h-16*s,12*s,nc_paper()?5:s,fg,bg);
        nc_icon(d->symbol?d->symbol:RISC_SYMBOL_WIFI,x+11*s,y+19*s,16*s,fg);
        nc_text(x+34*s,y+32*s,w-69*s,n->label,2,fg);
        cn_signal(x+w-31*s,y+20*s,14*s,(unsigned)n->value,fg,bg);
        nc_text(x+11*s,y+47*s,w-22*s,n->text,5,fg);
        static const char *const names[]={"SIGNAL","LINK SPEED","IP ADDRESS","ONLINE FOR"};
        for(unsigned i=0;i<4;i++){
            int cx=x+11*s+(int)(i%2)*(w-22*s)/2,cy=y+(60+(int)(i/2)*28)*s;
            char v[40];nc_choice(d->choices,i,v,sizeof(v));
            nc_text(cx,cy,(w-26*s)/2,names[i],5,fg);
            nc_text(cx,cy+14*s,(w-26*s)/2,v,3,fg);
        }
        if(n->action)nc_hit(x,y+8*s,w,h-16*s,HIT_ACTION,n,0,false);
    }else if(n->kind==RISC_COMPONENT_SIGNAL_HERO){
        cn_signal(x+w/2-17*s,y+5*s,26*s,(unsigned)n->value,fg,nc_bg());
        nc_center(x,y+58*s,w,n->text,4,fg);nc_center(x,y+77*s,w,n->label,1,fg);
    }else{
        nc_box(x+2*s,y+10*s,16*s,16*s,8*s,2*s,fg,n->value==2?fg:nc_bg());
        if(n->value==2)nc_icon(RISC_SYMBOL_CHECK,x+5*s,y+13*s,9*s,nc_bg());
        else if(n->value==1)nc_round(x+7*s,y+15*s,6*s,6*s,3*s,fg);
        nc_text(x+26*s,y+23*s,w-28*s,n->label,2,fg);
        if(n->text[0])nc_text(x+26*s,y+38*s,w-28*s,n->text,3,fg);
        fill(x,y+h-2*s,w,2*s,fg);
    }
    return true;
}
