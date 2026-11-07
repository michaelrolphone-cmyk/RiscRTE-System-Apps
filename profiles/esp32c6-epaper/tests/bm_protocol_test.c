#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bm_protocol.h"
static void check(const char *input,const char *expected){
 cJSON *v=cJSON_Parse(input);assert(v);char*s=bm_canonical(v);assert(s);assert(!strcmp(s,expected));free(s);cJSON_Delete(v);
}
int main(void){
 check("{\"z\":0,\"a\":[true,false,null,-0,42]}","{\"a\":[true,false,null,0,42],\"z\":0}");
 check("{\"s\":\"\\u00e9\\ud83d\\ude00\\u007f\\n/\"}","{\"s\":\"\\u00e9\\ud83d\\ude00\\u007f\\n/\"}");
 check("{\"\\ud800\\udc00\":1,\"\\ue000\":2,\"a\":3}","{\"a\":3,\"\\ue000\":2,\"\\ud800\\udc00\":1}");
 check("{\"arguments\":{\"provider\":\"abc\"},\"id\":\"r1\",\"operation\":\"provider.poll\",\"realm\":\"site\",\"site\":\"s\"}","{\"arguments\":{\"provider\":\"abc\"},\"id\":\"r1\",\"operation\":\"provider.poll\",\"realm\":\"site\",\"site\":\"s\"}");
 cJSON *bad=cJSON_Parse("{\"x\":1,\"x\":2}");assert(!bm_canonical(bad));cJSON_Delete(bad);
 bad=cJSON_Parse("{\"x\":1.5}");assert(!bm_canonical(bad));assert(!bm_json_integers_only(bad));cJSON_Delete(bad);
 bad=cJSON_Parse("{\"x\":9007199254740992}");assert(!bm_canonical(bad));cJSON_Delete(bad);
 check("{\"value\":9007199254740991}","{\"value\":9007199254740991}");
 puts("canonical JSON tests passed");
}
