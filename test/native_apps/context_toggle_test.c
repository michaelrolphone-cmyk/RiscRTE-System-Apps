#define main existing_context_client_cases
#include "context_client_test.c"
#undef main
int main(void){
 portable_contexts_client c;assert(portable_contexts_open(&c,&runtime));
 assert(portable_context_enabled_save(&kv,true));assert(portable_contexts_step(&c,true,true)&&last_policy.enabled);
 assert(portable_contexts_pause(&c));assert(portable_context_enabled_save(&kv,false));
 portable_contexts_settings_changed(&c,true);assert(portable_contexts_step(&c,true,true)&&!last_policy.enabled);
 unsigned before=writes;c.foreground_learning=true;assert(portable_contexts_step(&c,true,true)&&last_policy.enabled&&writes==before);
 c.foreground_learning=false;assert(portable_contexts_pause(&c));assert(portable_contexts_step(&c,true,true)&&!last_policy.enabled&&writes==before);
 bool enabled=true;assert(portable_context_enabled_load(&kv,&enabled)&&!enabled);
 assert(portable_contexts_close(&c)&&!grants);
 puts("Context toggle: off stops background policy; training is temporary and preserves saved off state PASS");
}
