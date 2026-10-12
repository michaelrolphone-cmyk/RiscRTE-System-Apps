/* Read-only inspection of the shipping scene host for boundary injection. */
#include SCENE_HOST_SOURCE
bool review_target(unsigned value,int*x,int*y){
 for(unsigned i=0;i<visible.count;i++){const hit*h=&visible.items[i];if(h->value==(int)value&&h->w>5&&h->h>5){*x=h->x+h->w/2;*y=h->y+h->h/2;return true;}}
 return false;
}
uint64_t review_scene_session(void){return session;}
