#ifndef PORTABLE_RGB565_BLEND_H
#define PORTABLE_RGB565_BLEND_H
#include <stdint.h>
/* Exact existing RGB888-over-RGB565 arithmetic, including truncation before
 * blending. These tables are floor(v*255/max), NOT bit replication. */
static const uint8_t portable_rgb565_expand5[32]={0,8,16,24,32,41,49,57,65,74,82,90,98,106,115,123,131,139,148,156,164,172,180,189,197,205,213,222,230,238,246,255};
static const uint8_t portable_rgb565_expand6[64]={0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,85,89,93,97,101,105,109,113,117,121,125,129,133,137,141,145,149,153,157,161,165,170,174,178,182,186,190,194,198,202,206,210,214,218,222,226,230,234,238,242,246,250,255};
static inline uint16_t portable_rgb565_pack(uint32_t rgb) {
 return (uint16_t)(((rgb>>19)&31u)<<11|((rgb>>10)&63u)<<5|((rgb>>3)&31u));
}
/* Exact floor(n/255) for 0<=n<=65025, the complete channel blend range. */
static inline unsigned portable_rgb565_div255(unsigned n) {return (n+1u+(n>>8))>>8;}
static inline uint16_t portable_rgb565_over(uint16_t before,uint32_t rgb,unsigned alpha) {
 if(!alpha)return before;
 if(alpha>=255)return portable_rgb565_pack(rgb);
 unsigned inverse=255-alpha;
 unsigned r=portable_rgb565_div255(portable_rgb565_expand5[before>>11]*inverse+((rgb>>16)&255u)*alpha);
 unsigned g=portable_rgb565_div255(portable_rgb565_expand6[(before>>5)&63u]*inverse+((rgb>>8)&255u)*alpha);
 unsigned b=portable_rgb565_div255(portable_rgb565_expand5[before&31u]*inverse+(rgb&255u)*alpha);
 return (uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
}
#endif
