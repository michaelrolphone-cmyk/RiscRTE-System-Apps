#pragma once
#include <string>
#include <cassert>
#include <cstdint>
/* Synthetic release metadata. No published artifact or flash image. */
inline std::string cohortFixture(uint32_t abi=1,uint32_t firmware=513,uint32_t store=0x4f0000) {
 const std::string repo="michaelrolphone-cmyk/RiscRTE-T-Watch-S3",version="1.2.0";
 const std::string layout=abi==1?"riscrte-paired-16m-v1":"riscrte-paired-appdata-v2";
 const std::string base="https://github.com/"+repo+"/releases/download/firmware-v"+version+"/";
 std::string json="{\"schema\":1,\"firmware\":{\"kind\":\"firmware\",\"version\":\""+version+
  "\",\"tag\":\"firmware-v"+version+"\",\"asset\":\"twatch-s3-launcher-"+version+".bin\",\"url\":\""+
  base+"twatch-s3-launcher-"+version+".bin\",\"size\":16777216,\"sha256\":\""+std::string(64,'f')+
  "\",\"ota\":{\"kind\":\"paired-cohort\",\"product\":\"twatch-s3\",\"version\":\""+version+
  "\",\"runtime_version\":\"0.1.33\",\"source_repo\":\""+repo+"\",\"source_revision\":\""+std::string(40,'c')+
  "\",\"layout\":\""+layout+"\",\"store_abi\":"+std::to_string(abi)+
  ",\"asset\":\"twatch-s3-cohort-"+version+".bin\",\"url\":\""+base+"twatch-s3-cohort-"+version+
  ".bin\",\"size\":"+std::to_string(firmware+store)+",\"sha256\":\""+std::string(64,'a')+
  "\",\"firmware_size\":"+std::to_string(firmware)+",\"firmware_sha256\":\""+std::string(64,'b')+
  "\",\"store_size\":"+std::to_string(store)+",\"store_sha256\":\""+std::string(64,'d')+"\"}},\"apps\":[]}";
#ifdef UPDATE_PRODUCT_X4
 for(const auto& pair:{std::pair<std::string,std::string>{"michaelrolphone-cmyk/RiscRTE-T-Watch-S3","michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO"},{"twatch-s3","xteink-x4-pro"}}){size_t at=0;while((at=json.find(pair.first,at))!=std::string::npos){json.replace(at,pair.first.size(),pair.second);at+=pair.second.size();}}
 json.insert(1,"\"product\":\"xteink-x4-pro\",\"source_repo\":\"michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO\",");
#endif
 return json;
}
inline void cohortReplace(std::string& json,const std::string& old,const std::string& replacement) {
 auto at=json.find(old);assert(at!=std::string::npos);json.replace(at,old.size(),replacement);
}
