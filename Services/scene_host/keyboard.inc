/* Plain-text topology belongs exclusively to the shared presenter. */
static const unsigned char keyboard_keys[4][4][11]={
 {{'Q','W','E','R','T','Y','U','I','O','P',0},{'A','S','D','F','G','H','J','K','L',0},{RISC_SCENE_KEY_LAYER,'Z','X','C','V','B','N','M',RISC_SCENE_KEY_BACKSPACE,0},{',',RISC_SCENE_KEY_SPACE,'.',RISC_SCENE_KEY_DONE,0}},
 {{'1','2','3','4','5','6','7','8','9','0',0},{'-','/',':',';','(',')','&','@','"',0},{RISC_SCENE_KEY_LAYER,'!','?','+','=','#','$','%',RISC_SCENE_KEY_BACKSPACE,0},{',',RISC_SCENE_KEY_SPACE,'.',RISC_SCENE_KEY_DONE,0}},
 {{'q','w','e','r','t','y','u','i','o','p',0},{'a','s','d','f','g','h','j','k','l',0},{RISC_SCENE_KEY_LAYER,'z','x','c','v','b','n','m',RISC_SCENE_KEY_BACKSPACE,0},{',',RISC_SCENE_KEY_SPACE,'.',RISC_SCENE_KEY_DONE,0}},
 {{'[',']','{','}','<','>','\\','|','^','_',0},{'`','~','\'','*','-','/',';',':','"',0},{RISC_SCENE_KEY_LAYER,'!','?','+','=','#','$','%',RISC_SCENE_KEY_BACKSPACE,0},{',',RISC_SCENE_KEY_SPACE,'.',RISC_SCENE_KEY_DONE,0}}
};
typedef struct { int x,y,w,h,center;unsigned key,row,index; } keyboard_key;
static unsigned keyboard_units(unsigned key){
    return key==RISC_SCENE_KEY_LAYER||key==RISC_SCENE_KEY_BACKSPACE?3:key==RISC_SCENE_KEY_SPACE?12:key==RISC_SCENE_KEY_DONE?4:2;
}
static bool keyboard_bounds(unsigned layer,unsigned index,keyboard_key *out){
    bool portrait=height>=width*3u/2u;unsigned original=index;
    for(unsigned row=0;row<4;row++){
        unsigned units=0;
        for(unsigned i=0;keyboard_keys[layer][row][i];i++)units+=keyboard_units(keyboard_keys[layer][row][i]);
        int xq=320+(20-(int)units)*104;
        for(unsigned i=0;keyboard_keys[layer][row][i];i++){
            unsigned key=keyboard_keys[layer][row][i];int wq=(int)keyboard_units(key)*208;
            int x=(xq+30)/10,w=(wq-45)/10;xq+=wq;
            if(index--==0){
                int y=portrait?372+(int)row*92:84+(int)row*30,kh=portrait?80:26;
                *out=(keyboard_key){x*(int)width/480,y*(int)height/(portrait?800:240),
                    w*(int)width/480,kh*(int)height/(portrait?800:240),2*x+w,key,row,original};return true;
            }
        }
    }
    return false;
}
static const char *keyboard_caption(unsigned layer,unsigned key,char ch[2]){
    static const char *const layers[]={"123","abc","#+=","ABC"};
    ch[0]=(char)key;ch[1]=0;
    return key==RISC_SCENE_KEY_LAYER?layers[layer]:key==RISC_SCENE_KEY_BACKSPACE?"DEL":
        key==RISC_SCENE_KEY_SPACE?"SPACE":key==RISC_SCENE_KEY_DONE?"DONE":ch;
}
