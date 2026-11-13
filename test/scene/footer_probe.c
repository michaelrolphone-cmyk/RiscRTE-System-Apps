#include SCENE_HOST_SOURCE
bool scene_footer_target(uint32_t action,int*x,int*y){
 for(unsigned i=0;i<visible.count;i++){const hit*h=&visible.items[i];int at=node_index(&document,h->node);
  if(at<0||document.nodes[at].action!=action||h->h<12)continue;
  for(unsigned j=i+1;j<visible.count;j++){const hit*b=&visible.items[j];
   if(h->x<b->x+b->w&&b->x<h->x+h->w&&h->y<b->y+b->h&&b->y<h->y+h->h)return false;
  }
  *x=h->x+h->w/2;*y=h->y+h->h/2;return true;
 }return false;
}
