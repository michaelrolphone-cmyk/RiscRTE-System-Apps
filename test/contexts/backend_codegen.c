#include "PortableContextsClient.h"
bool review_open(portable_contexts_client*c,const risc_runtime_api_v1*r){return portable_contexts_open(c,r);}
bool review_pause(portable_contexts_client*c){return portable_contexts_pause(c);}
bool review_close(portable_contexts_client*c){return portable_contexts_close(c);}
