// Decide once, then persist with the protected provisioning/activation journal.
// Capacity and refresh execute on the selected volume's copied worker. Retry
// after a journal exists always uses that exact medium, including absence/full.
risc_storage_package_request_v1 mediaProbeRequest; // provider-owned, outside the owner stack
unsigned mediaProbePhase;bool mediaProbeStarted,mediaProbeLocked;int mediaProbeOutcome;
int32_t selectMedium(const risc_storage_volume_api_v1*sd){
 if(installMediumSelected)return INSTALLED_APPS_OK;
 if(!mediaProbeStarted){
  installMedium=0;mediaProbeLocked=false;mediaProbePhase=0;
  for(unsigned i=0;i<currentCount;++i)if(!std::strcmp(current[i].view.id,installId)){installMedium=current[i].medium;mediaProbeLocked=true;}
  // Legacy pre-staged files keep their source. No silent cross-volume copy.
  if(!internalVolume||!mediaApi()||!(mediaApi()->media_mask&2)||!provisionJson[0]){installMediumSelected=true;return INSTALLED_APPS_OK;}
  mediaProbeStarted=true;
 }
 const auto*v=selectedVolume(installMedium,sd);const auto*d=v?risc_storage_volume_download(v):nullptr;
 auto fallback=[&]()->int32_t{
  if(!installMedium&&!mediaProbeLocked){installMedium=1;mediaProbePhase=0;return INSTALLED_APPS_WORKING;}
  mediaProbeStarted=false;return INSTALLED_APPS_UNAVAILABLE;
 };
 if(!d||d->download_version<2)return fallback();
 const auto*a=&d->base.base;
 if(!installSession){int rc=a->begin(v->context,&installSession);if(rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;if(rc==RISC_STORAGE_ASYNC_RETAINED){retained=true;return INSTALLED_APPS_RETAINED;}if(rc!=RISC_STORAGE_ASYNC_OK)return fallback();volume=v;}
 if(volume!=v)return INSTALLED_APPS_INVALID;
 if(mediaProbePhase<2){
  if(!installOperation){
   int rc;
   if(!mediaProbePhase){risc_storage_async_request_v1 q{};q.operation=RISC_STORAGE_ASYNC_REFRESH;rc=a->submit(v->context,installSession,&q,&installOperation);}
   else {mediaProbeRequest={};mediaProbeRequest.operation=RISC_STORAGE_PACKAGE_FREE_BYTES;rc=d->base.submit_package(v->context,installSession,&mediaProbeRequest,&installOperation);}
   if(rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
   if(rc!=RISC_STORAGE_ASYNC_OK){retained=true;return INSTALLED_APPS_RETAINED;}return INSTALLED_APPS_WORKING;
  }
  int rc=a->collect(v->context,installSession,installOperation,&installResult);if(rc==RISC_STORAGE_ASYNC_BUSY||rc==RISC_STORAGE_ASYNC_PENDING)return INSTALLED_APPS_WORKING;
  installOperation=0;if(rc==RISC_STORAGE_ASYNC_RETAINED){retained=true;return INSTALLED_APPS_RETAINED;}
  if(!mediaProbePhase&&rc==RISC_STORAGE_ASYNC_OK){mediaProbePhase=1;return INSTALLED_APPS_WORKING;}
  mediaProbeOutcome=INSTALLED_APPS_UNAVAILABLE;
  if(mediaProbePhase==1&&rc==RISC_STORAGE_ASYNC_OK&&installResult.size==sizeof(uint64_t)){
   uint64_t free=0;std::memcpy(&free,installResult.data,sizeof(free));
   // Round every immutable image independently at the largest supported FAT
   // allocation unit. Keep manifest copies, directory growth and KV headroom.
   uint64_t required=((uint64_t(installRecord.imageSize)+65535)/65536)*65536+65536;
   JsonDocument plan;if(deserializeJson(plan,provisionJson))return INSTALLED_APPS_INVALID;
   for(JsonObjectConst row:plan["providers"].as<JsonArrayConst>())required+=((uint64_t(row["size"].as<uint32_t>())+65535)/65536)*65536+65536;
   if(required<=free)mediaProbeOutcome=INSTALLED_APPS_OK;
  }
  mediaProbePhase=2;
 }
 int rc=a->end(v->context,installSession);if(rc==RISC_STORAGE_ASYNC_BUSY||rc==RISC_STORAGE_ASYNC_PENDING)return INSTALLED_APPS_WORKING;
 if(rc!=RISC_STORAGE_ASYNC_OK){retained=true;return INSTALLED_APPS_RETAINED;}installSession=installOperation=0;volume=nullptr;
 if(mediaProbeOutcome!=INSTALLED_APPS_OK)return fallback();
 installMediumSelected=true;mediaProbeStarted=false;return INSTALLED_APPS_OK;
}
