/* Dimensioned providers with the retained real timezone controller scenarios. */
#include <stdint.h>
static unsigned test_width,test_height,test_stride,test_size;
#include SETTINGS_TIMEZONE_FIXTURE
int main(int argc,char **argv) {
  assert(argc>=4);test_width=(unsigned)atoi(argv[1]);test_height=(unsigned)atoi(argv[2]);
  test_stride=test_width*2+6;test_size=test_stride*test_height;
  framebuffer=malloc(test_size);assert(framebuffer);memset(framebuffer,0xa5,test_size);
  int result=retained_timezone_main(argc-2,argv+2);
  const uint8_t*p=(const uint8_t*)framebuffer;
  for(unsigned y=0;y<test_height;y++)for(unsigned x=test_width*2;x<test_stride;x++)assert(p[y*test_stride+x]==0xa5);
  free(framebuffer);return result;
}
