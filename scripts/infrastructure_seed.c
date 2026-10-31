/* Offline product seed/update utility. Exact reviewed LittleFS2.1 core only.
 * Never accesses a device. Existing images are mounted, never autoformatted. */
#include "lfs.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define BYTES 524288u
static uint8_t image[BYTES];
static int rd(const struct lfs_config*c,lfs_block_t b,lfs_off_t o,void*p,lfs_size_t n){(void)c;if(b>=128||o+n>4096)return LFS_ERR_IO;memcpy(p,image+b*4096+o,n);return 0;}
static int wr(const struct lfs_config*c,lfs_block_t b,lfs_off_t o,const void*p,lfs_size_t n){(void)c;if(b>=128||o+n>4096)return LFS_ERR_IO;const uint8_t*v=p;for(lfs_size_t i=0;i<n;i++){if((image[b*4096+o+i]&v[i])!=v[i])return LFS_ERR_IO;image[b*4096+o+i]&=v[i];}return 0;}
static int er(const struct lfs_config*c,lfs_block_t b){(void)c;if(b>=128)return LFS_ERR_IO;memset(image+b*4096,255,4096);return 0;}static int sy(const struct lfs_config*c){(void)c;return 0;}
static void die(const char* m){fprintf(stderr,"%s\n",m);exit(2);}static size_t read_file(const char* path,uint8_t* out,size_t cap){FILE*f=fopen(path,"rb");if(!f)die("Cannot open input");size_t n=fread(out,1,cap,f);if(fgetc(f)!=EOF||ferror(f))die("Input exceeds bound");fclose(f);return n;}
int main(int argc,char**argv){if(argc!=5&&argc!=6)die("seed OUT POLICY STATE create|update [EXISTING_IMAGE]");struct lfs_config cfg={0};cfg.read=rd;cfg.prog=wr;cfg.erase=er;cfg.sync=sy;cfg.read_size=cfg.prog_size=128;cfg.block_size=4096;cfg.block_count=128;cfg.cache_size=512;cfg.lookahead_size=128;cfg.block_cycles=512;lfs_t fs={0};
 if(!strcmp(argv[4],"create")&&argc==5){memset(image,255,sizeof(image));if(lfs_format(&fs,&cfg))die("Format failed");}else if(!strcmp(argv[4],"update")&&argc==6){if(read_file(argv[5],image,sizeof(image))!=sizeof(image))die("Image geometry mismatch");}else die("Explicit create or update required");
 if(lfs_mount(&fs,&cfg))die("Existing volume is not mountable; no format attempted");int d=lfs_mkdir(&fs,"/n00000064");if(d&&d!=LFS_ERR_EXIST)die("Namespace failure");const char* names[]={"/n00000064/node-policy.json","/n00000064/node-state.bin"};uint8_t input[65536],verify[65536];
 for(int i=0;i<2;i++){size_t n=read_file(argv[i+2],input,i?65536:16384);if(!n)die("Empty seed file");lfs_file_t f={0};if(lfs_file_open(&fs,&f,names[i],LFS_O_WRONLY|LFS_O_CREAT|LFS_O_TRUNC)||lfs_file_write(&fs,&f,input,n)!=(lfs_ssize_t)n||lfs_file_sync(&fs,&f)||lfs_file_close(&fs,&f))die("File replace failed");}
 if(lfs_unmount(&fs)||lfs_mount(&fs,&cfg))die("Remount failed");struct lfs_fsinfo info;if(lfs_fs_stat(&fs,&info)||info.disk_version!=0x20001||info.block_size!=4096||info.block_count!=128)die("Unexpected disk format");
 for(int i=0;i<2;i++){size_t n=read_file(argv[i+2],input,sizeof(input));lfs_file_t f={0};if(lfs_file_open(&fs,&f,names[i],LFS_O_RDONLY)||lfs_file_size(&fs,&f)!=(lfs_soff_t)n||lfs_file_read(&fs,&f,verify,sizeof(verify))!=(lfs_ssize_t)n||memcmp(input,verify,n)||lfs_file_close(&fs,&f))die("Readback mismatch");}if(lfs_unmount(&fs))die("Unmount failed");int fd=open(argv[1],O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0||write(fd,image,sizeof(image))!=sizeof(image)||fsync(fd)||close(fd))die("Output already exists or write failed");puts("Seeded LittleFS2.1 image remounted and verified; no device operation.");return 0;}
