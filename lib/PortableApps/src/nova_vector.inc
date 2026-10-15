/* Bounded native scan converter for the original glyph outlines. No heap,
 * firmware access, surface scaling or font substitution. */
#include "../fonts/native_vectors.inc"
static uint8_t np_vector_data[NP_VECTOR_BYTES];
static bool np_vectors_ready;
static bool np_vector_unpack(void) {
 if(np_vectors_ready)return true;
 size_t in=0,out=0;
#ifdef PORTABLE_CONTEXTS_RASTER
 size_t checkpoint=512;
#endif
 while(out<NP_VECTOR_BYTES) {
  if(in>=sizeof(np_vector_packed))return false;
  unsigned flags=np_vector_packed[in++];
  for(unsigned bit=0;bit<8 && out<NP_VECTOR_BYTES;bit++) {
   if(flags&(1u<<bit)){if(in>=sizeof(np_vector_packed))return false;np_vector_data[out++]=np_vector_packed[in++];}
   else {
    if(in+1>=sizeof(np_vector_packed))return false;
    unsigned token=np_vector_packed[in]|np_vector_packed[in+1]<<8;in+=2;
    unsigned distance=(token>>4)+1u,length=(token&15u)+3u;
    if(distance>out || length>NP_VECTOR_BYTES-out)return false;
    while(length--){np_vector_data[out]=np_vector_data[out-distance];out++;}
   }
  }
#ifdef PORTABLE_CONTEXTS_RASTER
  if(out>=checkpoint){if(failed || !contexts_capture_checkpoint())return false;checkpoint=out+512;}
#endif
 }
 if(in!=sizeof(np_vector_packed))return false;
 np_vectors_ready=true;return true;
}
#define NP_VECTOR_EDGES 512u
#define NP_VECTOR_WIDTH 128u
/* Bounds are glyph-local Q8 coordinates. Active scan intersections use Q16. */
typedef struct { int32_t x,step; int16_t first,last; int8_t winding; } np_vector_edge;
static np_vector_edge np_vector_edges[NP_VECTOR_EDGES];
static int32_t np_vector_hits[NP_VECTOR_EDGES];
static int8_t np_vector_winding[NP_VECTOR_EDGES];
static uint16_t np_vector_coverage[NP_VECTOR_WIDTH];
static unsigned np_vector_edges_used;
static bool np_vector_valid;
static int np_vector_ceil64(int value) {return value>=0?(value+63)/64:value/64;}
static void np_vector_line(int x0,int y0,int x1,int y1) {
 if(y0==y1 || !np_vector_valid)return;
 if(np_vector_edges_used>=NP_VECTOR_EDGES){np_vector_valid=false;return;}
 int winding=y1>y0?1:-1;
 if(y0>y1){int v=x0;x0=x1;x1=v;v=y0;y0=y1;y1=v;}
 int first=np_vector_ceil64(y0-32),last=np_vector_ceil64(y1-32);
 if(first<0)first=0;
 if(last<=first)return;
 int dy=y1-y0,dx=x1-x0;
 int64_t x=(int64_t)x0*256+(int64_t)(first*64+32-y0)*dx*256/dy,step=(int64_t)dx*16384/dy;
 if(x<INT32_MIN || x>INT32_MAX || step<INT32_MIN || step>INT32_MAX || last>INT16_MAX){np_vector_valid=false;return;}
 np_vector_edges[np_vector_edges_used++]=(np_vector_edge){(int32_t)x,(int32_t)step,(int16_t)first,(int16_t)last,(int8_t)winding};
}
static int np_vector_abs(int value) {return value<0?-value:value;}
static void np_vector_quad(int x0,int y0,int x1,int y1,int x2,int y2,unsigned depth) {
 if(depth>=8 || (np_vector_abs(2*x1-x0-x2)<=64 && np_vector_abs(2*y1-y0-y2)<=64)) {np_vector_line(x0,y0,x2,y2);return;}
 int a=(x0+x1)/2,b=(y0+y1)/2,c=(x1+x2)/2,d=(y1+y2)/2,m=(a+c)/2,n=(b+d)/2;
 np_vector_quad(x0,y0,a,b,m,n,depth+1);np_vector_quad(m,n,c,d,x2,y2,depth+1);
}
static void np_vector_cubic(int x0,int y0,int x1,int y1,int x2,int y2,int x3,int y3,unsigned depth) {
 if(depth>=8 || (np_vector_abs(3*x1-2*x0-x3)<=64 && np_vector_abs(3*y1-2*y0-y3)<=64 &&
                 np_vector_abs(3*x2-x0-2*x3)<=64 && np_vector_abs(3*y2-y0-2*y3)<=64)) {np_vector_line(x0,y0,x3,y3);return;}
 int a=(x0+x1)/2,b=(y0+y1)/2,c=(x1+x2)/2,d=(y1+y2)/2,e=(x2+x3)/2,f=(y2+y3)/2;
 int g=(a+c)/2,h=(b+d)/2,i=(c+e)/2,j=(d+f)/2,m=(g+i)/2,n=(h+j)/2;
 np_vector_cubic(x0,y0,a,b,g,h,m,n,depth+1);np_vector_cubic(m,n,i,j,e,f,x3,y3,depth+1);
}
static int np_vector_read(const uint8_t **at,const uint8_t *end) {
 unsigned value=0,shift=0;
 do {
  if(*at>=end || shift>=21){np_vector_valid=false;return 0;}
  unsigned b=*(*at)++;value|=(b&127u)<<shift;shift+=7;
  if(!(b&128u))break;
 }while(true);
 return (int)(value>>1)^-(int)(value&1u);
}
static void np_vector_cover(int left,int right,int width) {
 if(left<0)left=0;
 if(right>width*256)right=width*256;
 while(left<right){int x=left/256,end=(x+1)*256;if(end>right)end=right;np_vector_coverage[x]+=(uint16_t)(end-left);left=end;}
}
static bool np_vector_draw(int x,int y,int w,int h,const np_vector_glyph *g,uint32_t rgb,unsigned opacity) {
 if(!g->length || w<1 || h<1 || !opacity)return true;
 if(w>(int)NP_VECTOR_WIDTH || h>128 || g->right<=g->left || g->top<=g->bottom)return false;
 if(x>=width() || y>=height() || x<=-w || y<=-h)return true;
 if(opacity>255)opacity=255;
 if(!np_vector_unpack() || (size_t)g->offset+g->length>NP_VECTOR_BYTES)return false;
 const uint8_t *p=np_vector_data+g->offset,*end=p+g->length;
 int source_x=0,source_y=0,last_x=0,last_y=0,start_x=0,start_y=0;
 int sx=w*65536/(g->right-g->left),sy=h*65536/(g->top-g->bottom);
 np_vector_edges_used=0;np_vector_valid=true;
 while(p<end && np_vector_valid) {
  unsigned op=*p++,points=op==0||op==1?1:op==2?2:op==4?3:0;
  int px[3]={0},py[3]={0};
  for(unsigned i=0;i<points;i++) {
   int dx=np_vector_read(&p,end),dy=np_vector_read(&p,end);
   if(!np_vector_valid)break;
   source_x+=dx;source_y+=dy;
   if(source_x<INT16_MIN || source_x>INT16_MAX || source_y<INT16_MIN || source_y>INT16_MAX){np_vector_valid=false;break;}
   int64_t vx=(int64_t)(source_x-g->left)*sx/256,vy=(int64_t)(g->top-source_y)*sy/256;
   if(vx < -131072 || vx>131072 || vy < -131072 || vy>131072){np_vector_valid=false;break;}
   px[i]=(int)vx;py[i]=(int)vy;
  }
  if(!np_vector_valid)break;
  if(op==0){start_x=last_x=px[0];start_y=last_y=py[0];}
  else if(op==1){np_vector_line(last_x,last_y,px[0],py[0]);last_x=px[0];last_y=py[0];}
  else if(op==2){np_vector_quad(last_x,last_y,px[0],py[0],px[1],py[1],0);last_x=px[1];last_y=py[1];}
  else if(op==4){np_vector_cubic(last_x,last_y,px[0],py[0],px[1],py[1],px[2],py[2],0);last_x=px[2];last_y=py[2];}
  else if(op==3){np_vector_line(last_x,last_y,start_x,start_y);last_x=start_x;last_y=start_y;}
  else np_vector_valid=false;
 }
 if(!np_vector_valid)return false;
 for(int row=0;row<h;row++) {
  memset(np_vector_coverage,0,(size_t)w*sizeof(np_vector_coverage[0]));
  for(int sample=row*4;sample<row*4+4;sample++) {
   unsigned count=0;
   for(unsigned i=0;i<np_vector_edges_used;i++) {
    np_vector_edge *e=&np_vector_edges[i];if(sample<e->first || sample>=e->last)continue;
    unsigned k=count++;int32_t hit=e->x;e->x+=e->step;
    while(k && np_vector_hits[k-1]>hit){np_vector_hits[k]=np_vector_hits[k-1];np_vector_winding[k]=np_vector_winding[k-1];k--;}
    np_vector_hits[k]=hit;np_vector_winding[k]=e->winding;
   }
   int winding=0,previous=0;
   for(unsigned i=0;i<count;i++){int hit=np_vector_hits[i]/256;if(winding)np_vector_cover(previous,hit,w);winding+=np_vector_winding[i];previous=hit;}
  }
  for(int col=0;col<w;col++)if(np_vector_coverage[col]) {
   unsigned alpha=(np_vector_coverage[col]+2u)/4u;if(alpha>255)alpha=255;
   np_pixel(x+col,y+row,rgb,alpha*opacity/255u);
  }
#ifdef PORTABLE_CONTEXTS_RASTER
  if((row+1)%4==0 || row+1==h)if(failed || !contexts_capture_checkpoint())return false;
#endif
 }
 return true;
}
