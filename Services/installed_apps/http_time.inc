// One opt-in transport dispatcher. Time is native HTTP-session state, never
// package policy, persisted UTC, a feed-selected server, or RTC authority.
#if defined(INSTALLED_APPS_TIME_SERVER) && (!defined(INSTALLED_APPS_FEED_URL) || !defined(INSTALLED_APPS_BOOT_PROFILES))
#error Explicit automatic time requires native-feed and boot-profile selection
#endif
[[maybe_unused]] bool explicitTimeHostname(const char*value){
 if(!value)return false;
 unsigned label=0,n=0;
 for(;n<=RISC_HTTP_TIME_SERVER_MAX&&value[n];++n){
  const unsigned char c=value[n];
  if(c=='.'){if(!label||value[n-1]=='-')return false;label=0;}
  else if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')||
          (!label&&c=='-')||++label>63)return false;
 }
 return n&&n<=RISC_HTTP_TIME_SERVER_MAX&&label&&value[n-1]!='-';
}
[[maybe_unused]] bool freshTimeAvailable(){
#ifdef INSTALLED_APPS_TIME_SERVER
 return explicitTimeHostname(INSTALLED_APPS_TIME_SERVER)&&risc_http_client_time_from_v1(packageHttp);
#else
 return false;
#endif
}
bool freshTimeActive(){return automaticFreshTime&&automaticPhase;}
int32_t transportOpen(const risc_http_request_v1*q,uint64_t*h,bool fresh){
 if(h)*h=0;
 if(!packageHttp||!q||!h)return RISC_HTTP_INVALID;
 if(fresh){
#ifdef INSTALLED_APPS_TIME_SERVER
  if(!freshTimeActive()||!freshTimeAvailable())return RISC_HTTP_CLOCK;
  const auto*time=risc_http_client_time_from_v1(packageHttp);
  auto request=*q;request.utc_seconds=0;
  return time->open_with_time(packageHttp->context,&request,INSTALLED_APPS_TIME_SERVER,h);
#else
  return RISC_HTTP_CLOCK;
#endif
 }
 return packageHttp->open(packageHttp->context,q,h);
}
[[maybe_unused]] const risc_http_client_v1 automaticHttp={1,sizeof(automaticHttp),nullptr,
 [](void*,const risc_http_request_v1*q,uint64_t*h)->int32_t{return transportOpen(q,h,freshTimeActive());},
 [](void*,uint64_t h,void*p,uint32_t n,uint32_t*out)->int32_t{return packageHttp->read(packageHttp->context,h,p,n,out);},
 [](void*,uint64_t h,risc_http_response_v1*out)->int32_t{return packageHttp->info(packageHttp->context,h,out);},
 [](void*,uint64_t h)->int32_t{return packageHttp->close(packageHttp->context,h);}};
