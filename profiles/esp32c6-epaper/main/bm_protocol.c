#include "bm_protocol.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifndef BM_CANONICAL_ONLY
#include "esp_random.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#endif
typedef struct {char *s;size_t n,cap;bool bad;} outbuf;
static void add(outbuf *b,const char *s,size_t n){
 if(b->bad)return;if(n>12000-b->n){b->bad=true;return;}
 if(b->n+n+1>b->cap){size_t cap=b->n+n+65;char *p=realloc(b->s,cap);if(!p){b->bad=true;return;}b->s=p;b->cap=cap;}
 memcpy(b->s+b->n,s,n);b->n+=n;b->s[b->n]=0;
}
static void ch(outbuf *b,char c){add(b,&c,1);}
static void esc(outbuf *b,uint32_t u){char s[7];snprintf(s,sizeof s,"\\u%04x",(unsigned)u);add(b,s,6);}
static void string(outbuf *b,const char *s){
 if(!s){b->bad=true;return;}ch(b,'"');const unsigned char *p=(const unsigned char*)s;
 while(*p&&!b->bad){
  uint32_t u=*p++;unsigned n=0;uint32_t min=0;
  if(u>=0xc2&&u<=0xdf){u&=31;n=1;min=0x80;}
  else if(u>=0xe0&&u<=0xef){u&=15;n=2;min=0x800;}
  else if(u>=0xf0&&u<=0xf4){u&=7;n=3;min=0x10000;}
  else if(u>=0x80){b->bad=true;break;}
  for(unsigned i=0;i<n;i++){if(!*p||(*p&0xc0)!=0x80){b->bad=true;break;}u=(u<<6)|(*p++&63);}
  if(b->bad||u<min||u>0x10ffff||(u>=0xd800&&u<=0xdfff)){b->bad=true;break;}
  switch(u){
   case '"':add(b,"\\\"",2);break;case '\\':add(b,"\\\\",2);break;
   case '\b':add(b,"\\b",2);break;case '\f':add(b,"\\f",2);break;
   case '\n':add(b,"\\n",2);break;case '\r':add(b,"\\r",2);break;case '\t':add(b,"\\t",2);break;
   default:if(u>=32&&u<=126)ch(b,(char)u);else if(u<=0xffff)esc(b,u);
    else{u-=0x10000;esc(b,0xd800+(u>>10));esc(b,0xdc00+(u&1023));}
  }
 }ch(b,'"');
}
static int compare(const void*a,const void*b){return strcmp((*(const cJSON*const*)a)->string,(*(const cJSON*const*)b)->string);}
static bool integer(double d){return isfinite(d)&&fabs(d)<=9007199254740991.0&&trunc(d)==d;}
static void value(outbuf*b,const cJSON*v,unsigned depth){
 if(!v||depth>32){b->bad=true;return;}
 if(cJSON_IsNull(v)){add(b,"null",4);return;}
 if(cJSON_IsBool(v)){if(cJSON_IsTrue(v))add(b,"true",4);else add(b,"false",5);return;}
 if(cJSON_IsString(v)){string(b,v->valuestring);return;}
 if(cJSON_IsNumber(v)){if(!integer(v->valuedouble)){b->bad=true;return;}char s[32];int n=snprintf(s,sizeof s,"%.0f",v->valuedouble==0?0:v->valuedouble);if(n<1||n>=32)b->bad=true;else add(b,s,n);return;}
 if(cJSON_IsArray(v)){ch(b,'[');bool first=true;const cJSON*c;for(c=v->child;c;c=c->next){if(!first)ch(b,',');first=false;value(b,c,depth+1);}ch(b,']');return;}
 if(!cJSON_IsObject(v)){b->bad=true;return;}
 size_t count=0;const cJSON*c;for(c=v->child;c;c=c->next){if(!c->string){b->bad=true;return;}if(++count>1024){b->bad=true;return;}}
 const cJSON**members=count?malloc(count*sizeof *members):NULL;if(count&&!members){b->bad=true;return;}
 size_t i=0;for(c=v->child;c;c=c->next)members[i++]=c;if(count)qsort(members,count,sizeof *members,compare);
 ch(b,'{');for(i=0;i<count;i++){if(i){if(!strcmp(members[i-1]->string,members[i]->string)){b->bad=true;break;}ch(b,',');}string(b,members[i]->string);ch(b,':');value(b,members[i],depth+1);}ch(b,'}');free(members);
}
char *bm_canonical(const cJSON*v){outbuf b={0};value(&b,v,0);if(b.bad){free(b.s);return NULL;}return b.s;}
static bool numbers(const cJSON*v,unsigned depth){
 if(!v||depth>32)return false;if(cJSON_IsNumber(v))return integer(v->valuedouble);
 if(cJSON_IsObject(v)||cJSON_IsArray(v)){for(const cJSON*c=v->child;c;c=c->next)if(!numbers(c,depth+1))return false;return true;}
 return cJSON_IsString(v)||cJSON_IsBool(v)||cJSON_IsNull(v);
}
bool bm_json_integers_only(const cJSON*v){return numbers(v,0);}
#ifndef BM_CANONICAL_ONLY
static int random_bytes(void *ctx,unsigned char *out,size_t n){(void)ctx;esp_fill_random(out,n);return 0;}
char *bm_sign_value(const cJSON*q,const char*pem){
 if(!q||!pem)return NULL;
 char *wire=bm_canonical(q);if(!wire)return false;
 unsigned char digest[32],signature[256],base64[345];size_t n=0,encoded=0;
 mbedtls_sha256((const unsigned char*)wire,strlen(wire),digest,0);free(wire);
 mbedtls_pk_context key;mbedtls_pk_init(&key);
 bool ok=mbedtls_pk_parse_key(&key,(const unsigned char*)pem,strlen(pem)+1,NULL,0,random_bytes,NULL)==0;
 ok=ok&&mbedtls_pk_get_type(&key)==MBEDTLS_PK_RSA&&mbedtls_pk_get_bitlen(&key)==2048;
 if(ok){mbedtls_rsa_set_padding(mbedtls_pk_rsa(key),MBEDTLS_RSA_PKCS_V15,MBEDTLS_MD_SHA256);
  ok=mbedtls_pk_sign(&key,MBEDTLS_MD_SHA256,digest,32,signature,sizeof signature,&n,random_bytes,NULL)==0&&n==256;}
 mbedtls_pk_free(&key);if(!ok)return false;
 if(mbedtls_base64_encode(base64,sizeof base64,&encoded,signature,n)||encoded!=344)return false;
 base64[encoded]=0;return strdup((char*)base64);
}
bool bm_sign_request(cJSON*q,const char*pem,const char*provider){
 if(!q||!provider||cJSON_GetObjectItemCaseSensitive(q,"auth"))return false;
 char *signature=bm_sign_value(q,pem);if(!signature)return false;
 cJSON *auth=cJSON_CreateObject();if(!auth){free(signature);return false;}
 bool ok=cJSON_AddStringToObject(auth,"actor",provider)&&cJSON_AddStringToObject(auth,"signature",signature);
 free(signature);if(!ok){cJSON_Delete(auth);return false;}
 if(!cJSON_AddItemToObject(q,"auth",auth)){cJSON_Delete(auth);return false;}return true;
}
#endif
