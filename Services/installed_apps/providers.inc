// The protected cache is the authority for an already admitted graph. Loading
// it does no media I/O: removable image availability remains a launch concern.
// Metadata is replayed before apps so their dependencies exist on cold boot.
int32_t loadProviders(JsonVariantConst rows){
 if(rows.isNull()){providerCount=0;return INSTALLED_APPS_OK;}
 if(!rows.is<JsonArrayConst>()||rows.size()>ProviderMax)return INSTALLED_APPS_INVALID;
 if(!rows.size()){providerCount=0;return INSTALLED_APPS_OK;}
 if(!providerApi()||rows.size()>native->provider_capacity)return INSTALLED_APPS_UNAVAILABLE;
 risc_install_provider_v1 records[ProviderMax]{};unsigned count=0;
 for(JsonObjectConst row:rows.as<JsonArrayConst>()){
  auto&p=providers[count];
  if(!row["manifest"].is<JsonObjectConst>()||measureJson(row["manifest"])>ManifestMax||
     !copy(p.path,sizeof(p.path),row["path"])||!row["instance"].is<uint64_t>())return INSTALLED_APPS_INVALID;
  serializeJson(row["manifest"],p.manifest,sizeof(p.manifest));p.instance=row["instance"];
  records[count++]={p.manifest,p.path,p.instance};
 }
 // Native admission validates the complete batch transactionally. A rejected
 // last record cannot partially publish the earlier records.
 int rc=native->providers(native->context,records,count,false);
 if(rc==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;
 if(rc!=RISC_INSTALL_OK)return INSTALLED_APPS_INVALID;
 providerCount=count;return INSTALLED_APPS_OK;
}
bool saveProviders(JsonDocument&doc){
 auto rows=doc["providers"].to<JsonArray>();
 for(unsigned i=0;i<providerCount;++i){
  JsonDocument manifest;if(deserializeJson(manifest,providers[i].manifest))return false;
  auto row=rows.add<JsonObject>();row["manifest"]=manifest.as<JsonObjectConst>();
  row["path"]=providers[i].path;row["instance"]=providers[i].instance;
 }
 return true;
}
