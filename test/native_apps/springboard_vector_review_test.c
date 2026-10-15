/* Independent scan-converter qualification. The exact production vector
 * implementation is included with a clipped alpha sink; complete adapter,
 * async custody and native RGB565 tests are separate suites. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static uint8_t canvas[132][132],saved[132][132];
static int width(void){return 132;}
static int height(void){return 132;}
static void np_pixel(int x,int y,uint32_t rgb,unsigned alpha){(void)rgb;assert(alpha<=255);if(x>=0&&y>=0&&x<width()&&y<height())canvas[y][x]=(uint8_t)alpha;}
#include "../../lib/PortableApps/src/nova_vector.inc"
static size_t put_at; static int put_x,put_y;
static void put_value(int v){unsigned n=((unsigned)v<<1)^(unsigned)(v>>31);while(n>=128){np_vector_data[put_at++]=(n&127)|128;n>>=7;}np_vector_data[put_at++]=(uint8_t)n;}
static void point(unsigned op,int x,int y){np_vector_data[put_at++]=(uint8_t)op;put_value(x-put_x);put_value(y-put_y);put_x=x;put_y=y;}
static void rect(int a,int b,int c,int d,bool reversed){point(0,a,b);if(reversed){point(1,a,d);point(1,c,d);point(1,c,b);}else{point(1,c,b);point(1,c,d);point(1,a,d);}np_vector_data[put_at++]=3;}
static void rejected(const np_vector_glyph*g,int w,int h) {
 memset(canvas,0xa5,sizeof(canvas));memcpy(saved,canvas,sizeof(canvas));
 assert(!np_vector_draw(0,0,w,h,g,0xffffff,255));
 assert(!memcmp(canvas,saved,sizeof(canvas))); /* No partially painted failure. */
}
int main(void){
 assert(np_vector_unpack()); unsigned max_edges=0,max_glyph=0,draws=0,max_active=0;int64_t max_mapping=0;
 const int sizes[]={1,2,7,13,32,64,96,128};
 const unsigned glyphs=sizeof(np_vector_glyphs)/sizeof(np_vector_glyphs[0]);
 for(unsigned i=0;i<glyphs;i++){
  const np_vector_glyph*g=&np_vector_glyphs[i];
  assert((size_t)g->offset+g->length<=NP_VECTOR_BYTES && g->units);
  if(g->length)assert(g->right>g->left && g->top>g->bottom);
  if(g->length){const uint8_t*p=np_vector_data+g->offset,*end=p+g->length;int x=0,y=0;np_vector_valid=true;
   while(p<end){unsigned op=*p++,points=op==0||op==1?1:op==2?2:op==4?3:0;assert(op<=4);
    for(unsigned j=0;j<points;j++){x+=np_vector_read(&p,end);y+=np_vector_read(&p,end);assert(np_vector_valid);int64_t mx=(int64_t)(x-g->left)*(128*65536/(g->right-g->left)),my=(int64_t)(g->top-y)*(128*65536/(g->top-g->bottom));if(llabs(mx)>max_mapping)max_mapping=llabs(mx);if(llabs(my)>max_mapping)max_mapping=llabs(my);assert(mx>=INT32_MIN&&mx<=INT32_MAX&&my>=INT32_MIN&&my<=INT32_MAX);}}
  }
  for(unsigned j=0;j<sizeof(sizes)/sizeof(sizes[0]);j++)for(unsigned k=0;k<3;k++){
   int w=sizes[j],h=k==0?w:k==1?1:128;memset(canvas,0,sizeof(canvas));assert(np_vector_draw(2,2,w,h,g,0xffffff,255));
   if(g->length){for(int sample=0;sample<h*4;sample++){unsigned active=0;for(unsigned ei=0;ei<np_vector_edges_used;ei++)if(sample>=np_vector_edges[ei].first && sample<np_vector_edges[ei].last)active++;if(active>max_active)max_active=active;}}
   if(g->length&&np_vector_edges_used>max_edges){max_edges=np_vector_edges_used;max_glyph=i;}
   memcpy(saved,canvas,sizeof(canvas));memset(canvas,0,sizeof(canvas));assert(np_vector_draw(2,2,w,h,g,0xffffff,255));assert(!memcmp(saved,canvas,sizeof(canvas)));
   memset(canvas,0,sizeof(canvas));assert(np_vector_draw(-7,-9,w,h,g,0xffffff,255));
   for(int y=0;y<121;y++)for(int x=0;x<123;x++)assert(canvas[y][x]==saved[y+11][x+9]);
   draws+=3;
  }
 }
 printf("PASS original glyphs: %u draws, max edges %u in glyph %u, max active intersections %u, max coordinate mapping product %lld\n",draws,max_edges,max_glyph,max_active,(long long)max_mapping);
 np_vector_glyph g={.offset=0,.left=0,.bottom=0,.right=100,.top=100,.units=100};
 for(unsigned reverse=0;reverse<2;reverse++){
  put_at=0;put_x=put_y=0;rect(0,0,100,100,false);rect(25,25,75,75,reverse);g.length=(uint16_t)put_at;memset(canvas,0,sizeof(canvas));assert(np_vector_draw(0,0,100,100,&g,0xffffff,255));
  for(int y=0;y<100;y++)for(int x=0;x<100;x++)assert(canvas[y][x]==(reverse&&x>=25&&x<75&&y>=25&&y<75?0:255));
 }
 puts("PASS analytic winding: opposite nested contour has exact 50x50 hole; same direction fills exactly");
 g.offset=NP_VECTOR_BYTES-1;g.length=2;rejected(&g,100,100);
 g.offset=0;g.length=1;np_vector_data[0]=9;rejected(&g,100,100);
 np_vector_data[0]=0;rejected(&g,100,100);
 g.length=5;np_vector_data[1]=np_vector_data[2]=np_vector_data[3]=np_vector_data[4]=255;rejected(&g,100,100);
 rejected(&g,129,100);rejected(&g,100,129);
 put_at=0;put_x=put_y=0;point(0,32768,0);g.length=(uint16_t)put_at;rejected(&g,100,100);
 put_at=0;put_x=put_y=0;point(0,32767,0);g.length=(uint16_t)put_at;g.right=1;rejected(&g,128,100);
 put_at=0;put_x=put_y=0;point(0,0,0);g.right=100;
 for(unsigned i=0;i<=NP_VECTOR_EDGES;i++)point(1,i%2?0:100,i%2?0:100);
 g.length=(uint16_t)put_at;rejected(&g,100,100);
 puts("PASS malformed bounds: source overrun, unknown op, truncated point, overlong varint, 129-pixel size, source/transformed coordinates and edge overflow rejected without painting");
 printf("VECTOR_REVIEW {\"glyphs\":%u,\"draws\":%u,\"widths\":[1,2,7,13,32,64,96,128],\"height_modes\":[\"square\",\"one_pixel\",\"128_pixels\"],\"repeat_equal\":true,\"negative_clipping_exact\":true,\"winding_cases\":2,\"malformed_cases\":9,\"max_edges\":%u,\"edge_capacity\":%u,\"max_edge_glyph\":%u,\"max_active_intersections\":%u,\"max_coordinate_product\":%lld}\n",glyphs,draws,max_edges,NP_VECTOR_EDGES,max_glyph,max_active,(long long)max_mapping);
 return 0;
}
