// Hardware behavior is injected here. The target build separately compiles
// the GOOUUU socket mapping and Espressif driver.
static bool attached;
static unsigned samples;
static int32_t sampleJpeg(uint8_t* dst, uint32_t capacity) {
 ++samples;
 if(!attached)return 0;
 assert(capacity==8192);
 if(dst){memset(dst,0,capacity);dst[0]=0xff;dst[1]=0xd8;dst[capacity-2]=0xff;dst[capacity-1]=0xd9;}
 return capacity;
}
static void optionalCamera(JsonDocument& cfg) {
 Node n;init(n,cfg);
 const risc_still_camera_v1 mock{1,sizeof(mock),sampleJpeg};
 n.camera=&mock;
 assert(n.cameraInventory()==0&&!n.cameraReady&&samples==1);
 assert(n.cameraOfferAt>ticks);
 n.poolsEstablished=true;
 assert(n.inventoryStep()==1&&n.request["arguments"]["offers"].size()==4);
 attached=true;ticks=n.cameraProbeAt;
 assert(n.cameraInventory()==1&&n.cameraReady&&samples==2);
 auto offers=n.request["arguments"]["offers"];
 assert(offers.size()==1&&offers[0]["offer"]["provider_id"]=="esp32s3-infra-camera-capture");
 assert(offers[0]["offer"]["observed_health"]=="healthy"&&offers[0]["offer"]["local_instance_id"]=="camera");
 assert(!n.relayAction());
 // Renew a healthy offer through several intervals without taking another frame.
 for(unsigned renewal=0;renewal<3;++renewal){
  ticks=n.cameraOfferAt;
  assert(n.cameraInventory()==1 && n.cameraReady && samples==2);
  std::string metadata;serializeJson(n.request,metadata);
  assert(metadata.find("base64_chunks")==std::string::npos);
  assert(metadata.find("image/jpeg")==std::string::npos);
  assert(n.journal.state["pending"].size()==0);
 }
 n.config["allocation"]["automatic"]=true;
 n.memory.internal_free=n.memory.internal_largest=n.memory.dma_free=n.memory.dma_largest=300000;
 n.memory.psram_free=n.memory.psram_largest=2000000;n.memory.stack_headroom=16384;
 n.leaseUntil=ticks+120000;n.rebootRecovery=false;
 auto job=n.journal.state["job"].to<JsonObject>();
 job["spec"]["capability"]["capability_id"]="camera.capture";job["spec"]["inputs"].to<JsonObject>();
 job["phase"]="running";job["boot_id"]=n.boot;job["attempt_id"]="capture-test";job["generation"]=1;
 assert(!n.roleReady()&&n.canReceiveWork());
 assert(n.jobStep()==1&&n.action==Action::Result&&samples==3);
 assert(n.journal.valid());
 auto out=n.journal.state["pending"][0]["request"]["arguments"]["candidate"]["output"];
 assert(out["bytes"]==8192&&out["width"]==160&&out["media_type"]=="image/jpeg");
 auto chunks=out["base64_chunks"].as<JsonArray>();assert(chunks.size()==11);
 assert(strncmp(chunks[0].as<const char*>(),"/9gA",4)==0);
 assert(strlen(chunks[10].as<const char*>())==684);
 attached=false;n.journal.state["pending"].to<JsonArray>();n.phase=Phase::Ready;
 assert(n.jobStep()==1&&n.action==Action::Stop&&!n.cameraReady);
 n.phase=Phase::Stopped;n.dispose();
}
