#include "PortablePaperGeometry.h"
__attribute__((visibility("default"))) bool paper_project_probe(int32_t x,int32_t len,int32_t offset,uint32_t extent,uint32_t reference,int32_t*p,int32_t*s){return portable_paper_project_axis(x,len,offset,extent,reference,p,s);}
