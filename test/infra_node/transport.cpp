#include "Transport.h"
#include <cassert>
static unsigned calls;static int open(void*,const risc_http_request_v2* r,uint64_t* token){if(!calls++){assert(r->utc_seconds==0&&r->time_server);}else assert(r->utc_seconds>=1800000000&&!r->time_server);*token=calls;return 0;}
int main(){Infra::Transport t;assert(t.initialize());t.api.open=open;assert(t.begin("https://example.test",1,nullptr,0,nullptr,0,4096,5000,"pool.ntp.org",100)==0);t.token=0;t.utc=1800000000;t.utcAt=100;assert(t.begin("https://example.test",1,nullptr,0,nullptr,0,4096,5000,"pool.ntp.org",2100)==0);t.token=0;t.dispose();puts("Transport: SNTP first request, cached UTC subsequent requests PASS");}
