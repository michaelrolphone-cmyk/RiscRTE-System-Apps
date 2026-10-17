#include "PortablePaperGeometry.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

static uint64_t intervals, pixels, boundaries, rejected;
static uint32_t random_state = 7;
static uint32_t next_random(void) {
    random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223);
    return random_state;
}
static void check(int32_t start, int32_t length, int32_t offset,
        uint32_t extent, uint32_t reference, bool exhaustive) {
    int32_t position = -123, size = -456;
    bool valid = portable_paper_project_axis(start,length,offset,extent,reference,
                                            &position,&size);
    ++intervals;
    int64_t first = (int64_t)start + offset, last = first + length;
    if (valid) {
        assert(position >= 0 && size > 0);
        assert((uint32_t)position < extent && (uint64_t)position + size <= extent);
    } else assert(position == -123 && size == -456);
    if (exhaustive) for (uint32_t pixel = 0; pixel < extent; ++pixel) {
        int64_t logical = (int64_t)pixel * reference / extent;
        bool hit = length > 0 && logical >= first && logical < last;
        bool paint = valid && pixel >= (uint32_t)position &&
                     pixel < (uint32_t)(position + size);
        assert(hit == paint); ++pixels;
    }
    if (valid) for (int i = -1; i <= 2; ++i) {
        int64_t pixel = i < 1 ? (int64_t)position + i :
                               (int64_t)position + size + i - 2;
        if (pixel < 0 || pixel >= extent) continue;
        int64_t logical = pixel * reference / extent;
        bool hit = logical >= first && logical < last;
        bool paint = pixel >= position && pixel < (int64_t)position + size;
        assert(hit == paint); ++boundaries;
    }
}
static void reject(int32_t length,uint32_t extent,uint32_t reference) {
    int32_t x=12,w=34;
    assert(!portable_paper_project_axis(0,length,0,extent,reference,&x,&w));
    assert(x==12 && w==34);++rejected;
}
int main(void) {
    static const uint32_t extents[]={1,2,7,319,320,410,479,480,481,640,700,800,801,1024,2048,65535};
    static const uint32_t references[]={480,800};
    for(unsigned r=0;r<2;++r) for(unsigned e=0;e<sizeof(extents)/sizeof(extents[0]);++e)
        for(unsigned i=0;i<80;++i)
            check((int32_t)(next_random()%2401)-800,(int32_t)(next_random()%801),
                  (int32_t)(next_random()%1601)-800,extents[e],references[r],true);
    /* Every reference-size span, including negative transition offsets, is exact. */
    for(int32_t offset=-800;offset<=800;offset+=17)
        for(int32_t start=-33;start<=833;start+=7) {
            check(start,1,offset,800,800,true);
            check(start,92,offset,800,800,true);
        }
    /* Individual one-row intervals may collapse; their union equals the span. */
    for(unsigned e=0;e<sizeof(extents)/sizeof(extents[0]);++e) {
        uint32_t extent=extents[e];
        for(uint32_t p=0;p<extent;++p) {
            uint32_t row=(uint64_t)p*800/extent;
            int32_t start,size;
            assert(portable_paper_project_axis((int32_t)row,1,0,extent,800,&start,&size));
            assert(p>=(uint32_t)start && p<(uint32_t)(start+size));++pixels;
        }
    }
    const int32_t extremes[]={INT32_MIN,INT32_MIN+1,-801,-800,-1,0,1,799,800,INT32_MAX-1,INT32_MAX};
    for(unsigned a=0;a<sizeof(extremes)/sizeof(extremes[0]);++a)
        for(unsigned b=0;b<sizeof(extremes)/sizeof(extremes[0]);++b) {
            check(extremes[a],INT32_MAX,extremes[b],65535,480,true);
            check(extremes[a],1,extremes[b],65535,UINT32_MAX,true);
        }
    reject(0,480,480);reject(-1,480,480);reject(INT32_MIN,480,480);
    reject(1,0,480);reject(1,65536,480);reject(1,UINT32_MAX,480);reject(1,480,0);
    int32_t x=0;
    assert(!portable_paper_project_axis(0,1,0,480,480,NULL,&x));++rejected;
    assert(!portable_paper_project_axis(0,1,0,480,480,&x,NULL));++rejected;
    printf("paper projection: intervals=%" PRIu64 " pixel_assertions=%" PRIu64
           " edge_assertions=%" PRIu64 " invalid_rejections=%" PRIu64 " PASS\n",
           intervals,pixels,boundaries,rejected);
    return 0;
}
