#include "../../Services/biome_node/NativeEncoding.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
using namespace BiomeRelay;
static void hex(const uint8_t bytes[32],char output[65]){
 const char* digits="0123456789abcdef";for(unsigned i=0;i<32;++i){output[i*2]=digits[bytes[i]>>4];output[i*2+1]=digits[bytes[i]&15];}output[64]=0;
}
static void vector(JsonDocument& input){
 char text[8192],digest[65];uint8_t bytes[32];NativeCanonical c(text,sizeof(text));c.value(input.as<JsonVariantConst>());assert(c.finish(bytes));hex(bytes,digest);
 JsonDocument row;row["input"]=input;row["canonical"]=text;row["sha256"]=digest;serializeJson(row,std::cout);std::cout<<"\n";
}
static void vectors(){
 JsonDocument a;a["z"]="line\n\t\"\\ \xc3\xa9 \xf0\x9f\x98\x80\x7f";a["a"]=true;a["negative"]=-123;a["empty"]=nullptr;
 a["quote\"name"]="\x01";vector(a);
 a.clear();a["site"]="f45540bf9910412680734d0613e84460";a["realm"]="site";a["id"]="request-1";a["operation"]="provider.result";
 a["arguments"]["provider"]=std::string(64,'a');a["arguments"]["operation"]="action-1";a["arguments"]["generation"]=9007199254740991ULL;
 a["arguments"]["state"]="succeeded";a["arguments"]["result"]["channel"]=6;a["arguments"]["result"]["on"]=false;vector(a);
 a.clear();a["challenge"]=std::string(64,'0');a["site"]="human.site";a["key"]=std::string(64,'b');vector(a);
 a.clear();a["arguments"]["inputs"]["steps"][0]["on"][0]=1;a["arguments"]["inputs"]["steps"][0]["off"][0]=2;
 a["arguments"]["inputs"]["steps"][0]["wait_ms"]=137;a["arguments"]["inputs"]["steps"][1]["tone_hz"]=0;vector(a);
 // Fixed nonsecret RSA public-number fixture, modulus2048 bits/exponent65537.
 // It exercises PSA PKCS#1 -> native SPKI conversion, not any device identity.
 uint8_t rsa[270]={0x30,0x82,0x01,0x0a,0x02,0x82,0x01,0x01,0x00};
 rsa[9]=0x80;for(unsigned i=10;i<264;++i)rsa[i]=0xab;rsa[264]=1;
 const uint8_t exponent[]={2,3,1,0,1};memcpy(rsa+265,exponent,sizeof(exponent));
 uint8_t spki[544];const size_t size=rsaSpki(rsa,sizeof(rsa),spki,sizeof(spki));assert(size==294);
 char pem[512],encoded[1024],digest[65];assert(publicPem(spki,size,pem,sizeof(pem)));assert(base64(spki,size,encoded,sizeof(encoded)));
 bytesDigest(spki,size,digest);JsonDocument row;row["spki"]=encoded;row["fingerprint"]=digest;row["pem"]=pem;
 serializeJson(row,std::cout);std::cout<<"\n";
}
int main(int argc,char**){
 if(argc>1){vectors();return 0;}
 uint8_t bytes[32];char text[1024],digest[65];JsonDocument input;
 input["b"]="\xc3\xa9\xf0\x9f\x98\x80";input["a"]=1;NativeCanonical c(text,sizeof(text));c.value(input.as<JsonVariantConst>());assert(c.finish(bytes));
 assert(!strcmp(text,"{\"a\":1,\"b\":\"\\u00e9\\ud83d\\ude00\"}"));
 JsonDocument empty;empty.to<JsonObject>();NativeCanonical known;known.value(empty.as<JsonVariantConst>());assert(known.finish(bytes));hex(bytes,digest);
 assert(!strcmp(digest,"44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a"));
 input.clear();input["bad"]=1.25;NativeCanonical floating;floating.value(input.as<JsonVariantConst>());assert(!floating.finish(bytes));
 input.clear();input["bad"]="\xc0\x80";NativeCanonical invalid;invalid.value(input.as<JsonVariantConst>());assert(!invalid.finish(bytes));
 input.clear();input["too"]="large";char tiny[2];NativeCanonical bounded(tiny,sizeof(tiny));bounded.value(input.as<JsonVariantConst>());assert(!bounded.finish(bytes));
 char encoded[9];assert(base64(reinterpret_cast<const uint8_t*>("foobar"),6,encoded,sizeof(encoded))==8&&!strcmp(encoded,"Zm9vYmFy"));
 assert(base64(reinterpret_cast<const uint8_t*>("f"),1,encoded,sizeof(encoded))==4&&!strcmp(encoded,"Zg=="));
 assert(!base64(reinterpret_cast<const uint8_t*>("foobar"),6,encoded,8));
 uint8_t shortKey[]={0x30,0x06,0x02,0x01,0x03,0x02,0x01,0x03},spki[64];
 const uint8_t expected[]={0x30,0x1a,0x30,0x0d,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01,0x05,0x00,0x03,0x09,0x00,0x30,0x06,0x02,0x01,0x03,0x02,0x01,0x03};
 assert(rsaSpki(shortKey,sizeof(shortKey),spki,sizeof(spki))==sizeof(expected)&&!memcmp(expected,spki,sizeof(expected)));
 assert(!rsaSpki(shortKey,sizeof(shortKey),spki,sizeof(expected)-1));
 puts("Biome native encoding: sorted canonical JSON, Unicode surrogate pairs, integral values, SHA256, Base64, and RSA SPKI format PASS");
}
