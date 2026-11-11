/* Offline read-only extraction. Never formats or writes the supplied volume. */
#include "lfs.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t image[524288],value[65536];
static int rd(const struct lfs_config*c,lfs_block_t b,lfs_off_t o,void*p,lfs_size_t n){(void)c;if(b>=128||o+n>4096)return LFS_ERR_IO;memcpy(p,image+b*4096+o,n);return 0;}
int main(int argc,char**argv){
 if(argc!=2)return 2;FILE* in=fopen(argv[1],"rb");if(!in)return 3;size_t n=fread(image,1,sizeof(image),in);int extra=fgetc(in);fclose(in);if(n!=sizeof(image)||extra!=EOF)return 4;
 struct lfs_config cfg={0};cfg.read=rd;cfg.read_size=cfg.prog_size=128;cfg.block_size=4096;cfg.block_count=128;cfg.cache_size=512;cfg.lookahead_size=128;cfg.block_cycles=512;lfs_t fs={0};if(lfs_mount(&fs,&cfg))return 5;
 const char* paths[]={"/n00000064/node-policy.json","/n00000064/node-state.bin"};
 for(unsigned i=0;i<2;++i){lfs_file_t f={0};if(lfs_file_open(&fs,&f,paths[i],LFS_O_RDONLY))return 6;lfs_soff_t size=lfs_file_size(&fs,&f);if(size<=0||size>(i?65536:16384))return 7;if(lfs_file_read(&fs,&f,value,size)!=size||lfs_file_close(&fs,&f))return 8;uint8_t length[4];for(unsigned j=0;j<4;++j)length[j]=size>>(8*j);if(fwrite(length,1,4,stdout)!=4||fwrite(value,1,size,stdout)!=(size_t)size)return 9;}
 return lfs_unmount(&fs)?10:0;
}
