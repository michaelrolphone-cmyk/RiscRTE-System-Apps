#pragma once
#include "Digest.h"
#include "../../lib/EnrollmentCrypto/monocypher.h"
namespace Infra {
inline void hexEncode(const uint8_t* in,size_t n,char* out){for(size_t i=0;i<n;++i)snprintf(out+i*2,3,"%02x",in[i]);}
inline bool hexDecode(const char* s,uint8_t* out,size_t n){if(!s||strlen(s)!=n*2)return false;for(size_t i=0;i<n;++i){unsigned v=0;for(unsigned j=0;j<2;++j){char c=s[i*2+j];if(c>='0'&&c<='9')v=v*16+c-'0';else if(c>='a'&&c<='f')v=v*16+c-'a'+10;else return false;}out[i]=v;}return true;}
inline void rawHash(const uint8_t* b,size_t n,uint8_t out[32]){dtls_sha256_ctx c;dtls_sha256_init(&c);dtls_sha256_update(&c,b,n);dtls_sha256_final(out,&c);crypto_wipe(&c,sizeof(c));}
inline void rawHmac(const uint8_t key[32],const uint8_t* b,size_t n,uint8_t out[32]){uint8_t pad[64]{},inner[32];memcpy(pad,key,32);for(auto& c:pad)c^=0x36;dtls_sha256_ctx ctx;dtls_sha256_init(&ctx);dtls_sha256_update(&ctx,pad,64);dtls_sha256_update(&ctx,b,n);dtls_sha256_final(inner,&ctx);for(auto& c:pad)c^=0x36^0x5c;dtls_sha256_init(&ctx);dtls_sha256_update(&ctx,pad,64);dtls_sha256_update(&ctx,inner,32);dtls_sha256_final(out,&ctx);crypto_wipe(pad,64);crypto_wipe(inner,32);crypto_wipe(&ctx,sizeof(ctx));}
inline bool enrollDecrypt(const char* secret,JsonObjectConst sealed,const char* aad,size_t aadSize,char* plaintext,size_t capacity,bool v2=false){
 uint8_t sk[32]{},pk[32]{},shared[32]{},salt[32]{},prk[32]{},key[32]{},nonce[12]{},cipher[1536]{};bool ok=false;crypto_aead_ctx ctx{};
 const char* encoded=sealed["ciphertext"]|"";size_t len=strlen(encoded)/2;
 if(len>=16&&len<capacity&&len<=sizeof(cipher)&&hexDecode(secret,sk,32)&&hexDecode(sealed["ephemeral_public_key"]|"",pk,32)&&hexDecode(sealed["nonce"]|"",nonce,12)&&hexDecode(encoded,cipher,len)){
  crypto_x25519(shared,sk,pk);uint8_t nz=0;for(auto b:shared)nz|=b;
  if(nz){rawHash(reinterpret_cast<const uint8_t*>(aad),aadSize,salt);rawHmac(salt,shared,32,prk);uint8_t info[]={'r','e','l','a','y','-','e','n','r','o','l','l','/','1',1};if(v2)info[13]='2';rawHmac(prk,info,sizeof(info),key);crypto_aead_init_ietf(&ctx,key,nonce);ok=crypto_aead_read(&ctx,reinterpret_cast<uint8_t*>(plaintext),cipher+len-16,reinterpret_cast<const uint8_t*>(aad),aadSize,cipher,len-16)==0;if(ok)plaintext[len-16]=0;}
 }
 crypto_wipe(sk,32);crypto_wipe(pk,32);crypto_wipe(shared,32);crypto_wipe(salt,32);crypto_wipe(prk,32);crypto_wipe(key,32);crypto_wipe(nonce,12);crypto_wipe(cipher,sizeof(cipher));crypto_wipe(&ctx,sizeof(ctx));return ok;
}
}
