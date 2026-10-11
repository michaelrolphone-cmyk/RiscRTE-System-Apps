/* NOVA-7 portrait presentation, owned by the shared scene capability.
 * Exact licensed font sizes/baselines and rounded outlines from the supplied
 * reference. Application-specific symbol/type controls remain with callers. */
#include "fonts/text.inc"
static void paper_fill(int x,int y,int w,int h,uint16_t color){
    int x1=x*(int)width/480,y1=y*(int)height/800;
    fill(x1,y1,(x+w)*(int)width/480-x1,(y+h)*(int)height/800-y1,color);
}
static void paper_round(int x,int y,int w,int h,int r,uint16_t color){
    if(layout_only)return;
    if(r>w/2)r=w/2;
    if(r>h/2)r=h/2;
    for(int row=0;row<h;row++){
        int yy=(y+row)*(int)height/800,yn=(y+row+1)*(int)height/800;
        if(yn<=(int)clip_top||yy>=(int)clip_bottom)continue;
        int dy=row<r?2*r-2*row-1:row>=h-r?2*(row-h+r)+1:0,inset=0;
        if(dy)while(inset<r){int dx=2*r-2*inset-1;if(dx*dx+dy*dy<=4*r*r)break;++inset;}
        paper_fill(x+inset,y+row,w-2*inset,1,color);
    }
}
static void paper_box(int x,int y,int w,int h,int r,bool inverse,int stroke){
    int inside=stroke/2,outside=stroke-inside;
    paper_round(x-outside,y-outside,w+2*outside,h+2*outside,r+outside,(uint16_t)profile->foreground_rgb565);
    if(!inverse)paper_round(x+inside,y+inside,w-2*inside,h-2*inside,r-inside>0?r-inside:0,(uint16_t)profile->background_rgb565);
}
static void paper_disk(int x,int y,int r,uint16_t color){
    if(layout_only)return;
    for(int row=-r;row<=r;row++){int span=0;while((span+1)*(span+1)+row*row<=r*r)++span;paper_fill(x-span,y+row,span*2+1,1,color);}
}
static int paper_abs(int a){return a<0?-a:a;}
static void paper_stroke(int x,int y,int x2,int y2,int stroke,uint16_t color){
    if(layout_only)return;
    int dx=paper_abs(x2-x),dy=-paper_abs(y2-y),sx=x<x2?1:-1,sy=y<y2?1:-1,error=dx+dy;
    for(;;){paper_disk(x,y,stroke/2,color);if(x==x2&&y==y2)break;int twice=2*error;if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}}
}
static unsigned paper_char(unsigned c){return c>=32&&c<=126?c:'?';}
static int paper_kern(const pc_font *f,unsigned a,unsigned b){
    unsigned key=a*128+b,lo=0,hi=f->kern_count;
    while(lo<hi){unsigned mid=lo+(hi-lo)/2,value=f->kern[mid].first*128u+f->kern[mid].second;if(value<key)lo=mid+1;else hi=mid;}
    return lo<f->kern_count&&f->kern[lo].first==a&&f->kern[lo].second==b?f->kern[lo].adjust:0;
}
static int paper_measure(const pc_font *f,const char *s,int tracking){
    int n=0;unsigned prev=0;
    for(unsigned i=0;s[i]&&i<RISC_SCENE_TEXT;i++){unsigned c=paper_char((unsigned char)s[i]);if(prev)n+=paper_kern(f,prev,c)+tracking*64;n+=f->glyphs[c-32].advance;prev=c;}
    return n;
}
static bool paper_bit(const pc_glyph *g,int x,int y){unsigned bit=(unsigned)y*g->width+(unsigned)x;return !!(g->bits[bit/8]&(128u>>(bit%8)));}
static void paper_text(const pc_font *f,int x,int baseline,const char *s,int tracking,int align,int limit,uint16_t color){
    if(layout_only)return;
    int total=paper_measure(f,s,tracking);if(total<=0)return;
    int span=(total+32)/64;if(limit&&span>limit)span=limit;
    int origin=x-(align==1?span/2:align==2?span:0),pen=0;unsigned prev=0;
    for(unsigned i=0;s[i]&&i<RISC_SCENE_TEXT;i++){
        unsigned c=paper_char((unsigned char)s[i]);const pc_glyph *g=&f->glyphs[c-32];
        if(prev)pen+=paper_kern(f,prev,c)+tracking*64;
        for(int row=0;row<g->height;row++){
            int yy=(baseline+g->top+row)*(int)height/800,yn=(baseline+g->top+row+1)*(int)height/800;
            if(yn<=(int)clip_top||yy>=(int)clip_bottom)continue;
            for(int col=0;col<g->width;){
            if(!paper_bit(g,col,row)){++col;continue;}int begin=col++;
            while(col<g->width&&paper_bit(g,col,row))++col;
            bool fitted=limit&&(total+32)/64>limit;
            int left=fitted?((pen+(g->left+begin)*64)*span+total/2)/total:(pen+32)/64+g->left+begin;
            int right=fitted?((pen+(g->left+col)*64)*span+total/2)/total:(pen+32)/64+g->left+col;
            if(right>left)paper_fill(origin+left,baseline+g->top+row,right-left,1,color);
        }
        }
        pen+=g->advance;prev=c;
    }
}
static void paper_hit(int x,int y,int w,int h,uint32_t kind,const risc_scene_node_v1 *n,int value){
    add_hit(x*(int)width/480,y*(int)height/800,w*(int)width/480,h*(int)height/800,kind,n?n->id:0,0,value);
}
static void paper_keyboard(const risc_scene_node_v1 *n){
    uint16_t fg=(uint16_t)profile->foreground_rgb565,bg=(uint16_t)profile->background_rgb565;
    paper_disk(56,46,28,fg);paper_disk(56,46,24,bg);
    paper_stroke(58,37,50,46,4,fg);paper_stroke(50,46,58,55,4,fg);paper_hit(28,18,56,56,HIT_BACK,NULL,0);
    int r=route_index(&document,current_route());
    if(r>=0)paper_text(&PCF_O900_26,96,56,document.routes[r].title,3,0,284,fg);
    paper_disk(424,46,28,fg);paper_disk(424,46,24,bg);
    paper_stroke(412,45,424,34,3,fg);paper_stroke(424,34,436,45,3,fg);
    paper_stroke(415,43,415,58,3,fg);paper_stroke(415,58,433,58,3,fg);paper_stroke(433,58,433,43,3,fg);
    paper_hit(396,18,56,56,HIT_HOME,NULL,0);paper_fill(32,106,416,4,fg);
    char count[24];size_t len=strlen(n->text);
    if(n->target)snprintf(count,sizeof(count),"%u / %u",(unsigned)len,(unsigned)n->target);
    else snprintf(count,sizeof(count),"%u",(unsigned)len);
    paper_text(&PCF_R700_16,32,142,"TEXT",8,0,200,fg);paper_text(&PCF_R700_18,448,142,count,2,2,180,fg);
    paper_box(32,152,328,80,10,false,4);
    const pc_font *font=len>10?&PCF_O900_30:len>7?&PCF_O900_38:&PCF_O900_46;
    unsigned size=len>10?30:len>7?38:46;
    /* Long drafts show their insertion end at a legible size, rather than
     * crushing an arbitrary 71-character value into a 328-pixel field. */
    const char *tail=n->text;while(*tail&&(paper_measure(font,tail,0)+32)/64>286)++tail;
    paper_text(font,48,192+(int)size*36/100,tail,0,0,286,fg);
    int cursor=(paper_measure(font,tail,0)+32)/64;paper_fill(52+cursor,174,5,40,fg);
    paper_box(368,152,80,80,10,false,4);paper_text(&PCF_R700_17,408,199,"CLEAR",2,1,76,fg);
    if(!(n->flags&RISC_SCENE_DISABLED))paper_hit(368,152,80,80,HIT_VALUE,n,RISC_SCENE_KEY_CLEAR);
    if(n->flags&RISC_SCENE_DISABLED){
        paper_text(&PCF_R700_20,240,286,"HARDWARE KEYBOARD",3,1,416,fg);
        paper_text(&PCF_R700_20,240,330,"ENTER done / ESC cancel",1,1,416,fg);return;
    }
    static const char *const layers[]={"UPPERCASE","NUMBERS + SYMBOLS","LOWERCASE","MORE SYMBOLS"};
    paper_text(&PCF_R700_18,240,298,layers[(unsigned)n->value],3,1,416,fg);
    paper_text(&PCF_R700_18,240,358,"LETTERS - NUMBERS - SYMBOLS",2,1,416,fg);
    for(unsigned i=0;;i++){
        keyboard_key key;if(!keyboard_bounds((unsigned)n->value,i,&key))break;
        int x=key.x*480/(int)width,y=372+(int)key.row*92,w=key.w*480/(int)width,cx=x+w/2;
        bool selected=n->id==path.focus[path.depth-1]&&focus_part==i;
        bool pressed=contact&&!moved&&down_x>=key.x&&down_y>=key.y&&down_x<key.x+key.w&&down_y<key.y+key.h;
        bool inverse=(key.key==RISC_SCENE_KEY_DONE)!=pressed;
        paper_box(x,y,w,80,12,inverse,selected?6:4);uint16_t ink=inverse?bg:fg;
        char ch[2];const char *caption=keyboard_caption((unsigned)n->value,key.key,ch);
        if(key.key==RISC_SCENE_KEY_BACKSPACE){
            paper_stroke(cx-14,y+40,cx-6,y+30,3,ink);paper_stroke(cx-6,y+30,cx+14,y+30,3,ink);
            paper_stroke(cx+14,y+30,cx+14,y+50,3,ink);paper_stroke(cx+14,y+50,cx-6,y+50,3,ink);
            paper_stroke(cx-6,y+50,cx-14,y+40,3,ink);paper_stroke(cx-3,y+35,cx+7,y+45,3,ink);paper_stroke(cx+7,y+35,cx-3,y+45,3,ink);
        }else if(key.key==RISC_SCENE_KEY_SPACE)paper_text(&PCF_R700_20,cx,y+47,caption,6,1,w-4,ink);
        else if(key.key==RISC_SCENE_KEY_DONE)paper_text(&PCF_O700_22,cx,y+48,caption,0,1,w-4,ink);
        else if(key.key==RISC_SCENE_KEY_LAYER)paper_text(&PCF_O700_20,cx,y+47,caption,0,1,w-4,ink);
        else paper_text(&PCF_O700_28,cx,y+50,caption,0,1,w-4,ink);
        add_hit(key.x,key.y,key.w,key.h,HIT_VALUE,n->id,0,(int32_t)key.key);
    }
}
