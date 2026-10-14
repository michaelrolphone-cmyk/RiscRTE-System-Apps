/* Compact adaptation of the supplied NOVA-7 keyboard: the same Orbitron /
 * Rajdhani assets, outlined rounded keys, circular controls and inverse Done.
 * Input topology remains in keyboard.inc and uses the current display extent. */
/* Orbitron has no caret glyph. Keep printable ASCII legible using the
 * existing Rajdhani face for that character, as the portrait reference does. */
static const pc_glyph *compact_glyph(unsigned role,unsigned ch){
    return &(ch=='^'?nc_font(2):nc_font(role))->glyphs[ch-32];
}
static int compact_measure(unsigned role,const char *s){
    int pen=0;for(unsigned i=0;s[i]&&i<RISC_SCENE_TEXT;i++)pen+=compact_glyph(role,paper_char((unsigned char)s[i]))->advance+nc_tracking(role);
    return (pen+32)/64;
}
static void compact_text(int x,int baseline,int maximum,const char *s,unsigned role,uint16_t color){
    if(layout_only||maximum<=0)return;
    int pen=0;
    for(unsigned i=0;s[i]&&i<RISC_SCENE_TEXT;i++){
        const pc_glyph *g=compact_glyph(role,paper_char((unsigned char)s[i]));
        if((pen+g->advance+32)/64>maximum)break;
        int first=(int)clip_top-baseline-g->top,last=(int)clip_bottom-baseline-g->top;
        if(first<0)first=0;
        if(last>g->height)last=g->height;
        for(int row=first;row<last;row++)for(int col=0;col<g->width;col++)
            if(paper_bit(g,col,row))pixel(x+(pen+32)/64+g->left+col,baseline+g->top+row,color);
        pen+=g->advance+nc_tracking(role);
    }
}
static void compact_center(int x,int baseline,int w,const char *s,unsigned role,uint16_t color){
    int n=compact_measure(role,s);compact_text(x+(n<w?(w-n)/2:0),baseline,w,s,role,color);
}
static void compact_keyboard_header(void){
    int pad=(int)profile->padding,top=(int)top_height(),button_width=(int)(profile->font_scale*30u+8u);
    uint16_t fg=(uint16_t)profile->foreground_rgb565,bg=(uint16_t)profile->background_rgb565;
    int size=top-8,left=pad+(button_width-size)/2,right=(int)width-pad-(button_width+size)/2;
    nc_box(left,4,size,size,size/2,1,fg,bg);nc_icon(RISC_SYMBOL_BACK,left+5,9,size-10,fg);
    nc_box(right,4,size,size,size/2,1,fg,bg);nc_icon(RISC_SYMBOL_PLACE,right+5,9,size-10,fg);
    add_hit(pad,3,button_width,top-6,HIT_BACK,0,0,0);
    add_hit((int)width-pad-button_width,3,button_width,top-6,HIT_HOME,0,0,0);
    int r=route_index(&document,current_route());
    if(r>=0)compact_center(pad+button_width+4,(top+nc_size(10))/2,(int)width-2*(pad+button_width+4),document.routes[r].title,1,fg);
    fill(pad,top-1,(int)width-2*pad,1,fg);
}
static void compact_key_box(const keyboard_key *key,bool inverse,bool selected,uint16_t fg,uint16_t bg){
    int border=selected?2:1,radius=key->h/6;
    if(inverse)nc_round(key->x,key->y,key->w,key->h,radius,fg);
    else nc_box(key->x,key->y,key->w,key->h,radius,border,fg,bg);
}
static void draw_keyboard(const risc_scene_node_v1 *n,int y,int h){
    (void)y;(void)h;
    uint16_t fg=(uint16_t)profile->foreground_rgb565,bg=(uint16_t)profile->background_rgb565;
    keyboard_key visibility;bool password=keyboard_visibility_bounds(n,&visibility);
    int x=32*(int)width/480,fy=42*(int)height/240,fh=32*(int)height/240;
    int fw=(password?272:328)*(int)width/480;
    nc_box(x,fy,fw,fh,5*nc_scale(),1,fg,bg);
    const char *tail=n->text;while(*tail&&compact_measure(0,tail)>fw-12*nc_scale())++tail;
    int baseline=fy+(fh+nc_size(10))/2;
    compact_text(x+4*nc_scale(),baseline,fw-8*nc_scale(),tail,0,fg);
    int cursor=x+4*nc_scale()+compact_measure(0,tail);
    fill(cursor+1,baseline-nc_size(10),nc_scale(),nc_size(12),fg);
    keyboard_key control=password?visibility:(keyboard_key){368*(int)width/480,fy,80*(int)width/480,fh,0,RISC_SCENE_KEY_CLEAR,0,0};
    compact_key_box(&control,false,password&&n->id==path.focus[path.depth-1]&&focus_part==KEYBOARD_VISIBILITY_INDEX,fg,bg);
    const char *caption=password?keyboard_visibility_caption(n):"CLEAR";
    compact_center(control.x,control.y+(control.h+nc_size(7))/2,control.w,caption,password?5:3,fg);
    if(password||!(n->flags&RISC_SCENE_DISABLED))add_hit(control.x,control.y,control.w,control.h,HIT_VALUE,n->id,0,(int32_t)control.key);
    if(n->flags&RISC_SCENE_DISABLED){
        compact_center(x,105*(int)height/240,(int)width-2*x,"HARDWARE KEYBOARD",1,fg);
        compact_center(x,130*(int)height/240,(int)width-2*x,"ENTER done / ESC cancel",3,fg);return;
    }
    for(unsigned i=0;;i++){
        keyboard_key key;if(!keyboard_bounds((unsigned)n->value,i,&key))break;
        bool selected=n->id==path.focus[path.depth-1]&&focus_part==i;
        bool pressed=contact&&!moved&&down_x>=key.x&&down_y>=key.y&&down_x<key.x+key.w&&down_y<key.y+key.h;
        bool inverse=(key.key==RISC_SCENE_KEY_DONE)!=pressed;
        compact_key_box(&key,inverse,selected,fg,bg);uint16_t ink=inverse?bg:fg;
        char ch[2];const char *label=keyboard_caption((unsigned)n->value,key.key,ch);
        unsigned role=key.key>=RISC_SCENE_KEY_LAYER?5:0;
        if(key.key==RISC_SCENE_KEY_BACKSPACE){
            int cx=key.x+key.w/2,cy=key.y+key.h/2,sz=nc_scale()*5;
            nc_line(cx-sz,cy,cx,cy-sz,1,ink);nc_line(cx-sz,cy,cx,cy+sz,1,ink);
            nc_line(cx,cy-sz,cx+sz,cy-sz,1,ink);nc_line(cx+sz,cy-sz,cx+sz,cy+sz,1,ink);nc_line(cx+sz,cy+sz,cx,cy+sz,1,ink);
        }else compact_center(key.x,key.y+(key.h+nc_size(role==0?10:7))/2,key.w,label,role,ink);
        add_hit(key.x,key.y,key.w,key.h,HIT_VALUE,n->id,0,(int32_t)key.key);
    }
    static const char *const layers[]={"UPPERCASE","NUMBERS + SYMBOLS","LOWERCASE","MORE SYMBOLS"};
    compact_center(x,220*(int)height/240,(int)width-2*x,layers[(unsigned)n->value],1,fg);
}
