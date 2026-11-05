// Uses the real Client, RemoteVolume, Digest, server and storage adapters above.
struct ReplacementServerFault {
 Store store;RiscUserFiles::Volume volume;risc_storage_volume_api_v1_fs api{};
 unsigned mode=0,replacements=0;static ReplacementServerFault*active;
 ReplacementServerFault(){active=this;assert(volume.begin(&store.api,{nullptr,[](void*){return true;},malloc,free}));api=*reinterpret_cast<const risc_storage_volume_api_v1_fs*>(volume.api());
  auto&ext=api.state.prepared.base.sleep.terminal.power.volume;
  ext.base.file_write=[](void*c,uint32_t h,const void*b,size_t n){auto&f=*active;if(f.mode==1)return size_t(0);return f.volume.api()->file_write(c,h,b,n);};
  ext.file_sync=[](void*c,uint32_t h){auto&f=*active;return f.mode!=2&&risc_storage_volume_extension(f.volume.api())->file_sync(c,h);};
  api.replace_file=[](void*c,const char*from,const char*to){auto&f=*active;++f.replacements;if(f.mode==3)return false;bool result=risc_storage_volume_fs(f.volume.api())->replace_file(c,from,to);return f.mode!=4&&result;};
 }
 const risc_storage_volume_api_v1*base(){return &api.state.prepared.base.sleep.terminal.power.volume.base;}
 ~ReplacementServerFault(){assert(volume.end());active=nullptr;}
};
ReplacementServerFault*ReplacementServerFault::active=nullptr;
[[maybe_unused]] static void confirmedReplacementTests(){
 auto terminal=[](RemoteVolume&r){return r.state()==RemoteVolume::State::Done||r.state()==RemoteVolume::State::Failed||r.state()==RemoteVolume::State::Retained;};
 auto tick=[](Fixture&f,RemoteVolume&r){unsigned before=f.link.calls;r.tick(f.now);assert(f.link.calls-before<=1);f.server.tick(f.now++);};
 auto pump=[&](Fixture&f,RemoteVolume&r){for(unsigned i=0;i<30000&&!terminal(r);i++)tick(f,r);assert(terminal(r));};
 auto contents=[](const risc_storage_volume_api_v1*a,const char*p){uint64_t size=0;auto h=a->file_open_read(a->context,p,&size);if(!h)return std::string();std::string out;char b[512];for(size_t n;(n=a->file_read(a->context,h,b,sizeof(b)));)out.append(b,n);assert(out.size()==size&&a->file_close(a->context,h,true));return out;};
 // A confirmation is scoped to exactly one replacement; later ordinary PUTs
 // return to exclusive creation. Generic HEAD has no fabricated revision.
 {Fixture f;AtomicLocal local;assert(f.request("PUT","/target",nullptr,"original")==201);RemoteVolume r(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});
  assert(r.upload(local.volume(),"/models/model.stl","/target",f.now));pump(f,r);assert(r.state()==RemoteVolume::State::Failed&&f.client.status()==412&&contents(f.volume.api(),"/target")=="original");
  auto serial=f.link.serial;assert(r.upload(local.volume(),"/models/model.stl","/target",f.now,true));pump(f,r);assert(r.state()==RemoteVolume::State::Done&&f.link.serial==serial+2&&contents(f.volume.api(),"/target")=="readonly source");assert(f.link.up.find("If-Match: *\r\n")!=std::string::npos&&f.link.up.find("If-None-Match:")==std::string::npos);
  assert(r.upload(local.volume(),"/models/model.stl","/target",f.now));pump(f,r);assert(r.state()==RemoteVolume::State::Failed&&f.client.status()==412&&f.link.up.find("If-None-Match: *\r\n")!=std::string::npos);
  assert(!f.client.request("GET","/target",nullptr,0,f.io(),f.now,nullptr,0,Client::PutMode::ConfirmedReplacement));assert(!f.client.request("PUT","/target",nullptr,0,f.io(),f.now,"\"revision\"",0,Client::PutMode::ConfirmedReplacement));
  assert(r.close());
 }
 // Watch supplies a real ETag; no wildcard fallback on stale revisions.
 for(bool stale:{false,true}){AtomicLocal destination;destination.files.records[{64,"model.obj"}]={"original",7};Fixture f(destination.volume());AtomicLocal local;RemoteVolume r(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});auto serial=f.link.serial;
  assert(r.upload(local.volume(),"/models/model.stl","/models/model.obj",f.now,true));
  for(unsigned i=0;i<30000&&!(f.link.serial==serial+1&&f.client.state()==Client::State::Connect);i++)tick(f,r);
  assert(f.link.serial==serial+1&&f.client.state()==Client::State::Connect);
  if(stale)destination.files.records[{64,"model.obj"}]={"concurrent edit",8};
  pump(f,r);assert(f.link.up.find("If-Match: \"risc-0000000000000007\"\r\n")!=std::string::npos&&f.link.up.find("If-Match: *")==std::string::npos);
  assert(r.state()==(stale?RemoteVolume::State::Failed:RemoteVolume::State::Done));assert(destination.files.records.at({64,"model.obj"}).bytes==(stale?"concurrent edit":"readonly source"));assert(destination.files.replacements==unsigned(!stale));assert(r.close());
 }
 // Failed HEAD or invalid/weak ETag never starts a PUT. Empty ETag header is
 // distinct from no header and is an invalid response, not a fallback signal.
 for(const char*reply:{"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n","HTTP/1.1 500 Error\r\nContent-Length: 0\r\n\r\n","HTTP/1.1 200 OK\r\nContent-Length: 5\r\nETag: W/\"weak\"\r\n\r\n","HTTP/1.1 200 OK\r\nContent-Length: 5\r\nETag: invalid\r\n\r\n","HTTP/1.1 200 OK\r\nContent-Length: 5\r\nETag: \r\n\r\n"}){
  RawPeer f;AtomicLocal local;assert(f.remote.upload(local.volume(),"/models/model.stl","/target",f.now,true));
  for(unsigned i=0;i<10000&&f.client.state()!=Client::State::Reply;i++)f.tick();assert(f.client.state()==Client::State::Reply);auto serial=f.link.serial;f.link.down=reply;f.link.accepted=true;
  for(unsigned i=0;i<10000&&!terminal(f.remote);i++)f.tick();assert(f.remote.state()==RemoteVolume::State::Failed&&f.link.serial==serial&&f.link.up.find("PUT ")==std::string::npos);assert(f.remote.close());
 }
 // Every cancellation boundary keeps the old destination until publication,
 // drains the source, and never replays a request. A late Cancel cannot undo
 // an acknowledged PUT while only the local read handle remains to close.
 for(unsigned phase=0;phase<10;phase++){
  Fixture f;AtomicLocal local;local.files.records[{64,"model.stl"}]={std::string(6000,'r'),1};assert(f.request("PUT","/target",nullptr,"original")==201);RemoteVolume r(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});auto serial=f.link.serial;assert(r.upload(local.volume(),"/models/model.stl","/target",f.now,true));
  auto reached=[&](){auto s=f.client.state();switch(phase){case 0:return r.state()==RemoteVolume::State::Prepare;case 1:return s==Client::State::Header&&f.link.serial==serial+1;case 2:return s==Client::State::Reply&&f.link.serial==serial+1;case 3:return s==Client::State::Connect&&f.link.serial==serial+1;case 4:return s==Client::State::Header&&f.link.serial==serial+2&&f.link.up.empty();case 5:return s==Client::State::Header&&f.link.serial==serial+2&&!f.link.up.empty();case 6:return s==Client::State::UploadSend&&f.link.serial==serial+2;case 7:return s==Client::State::Reply&&f.link.serial==serial+2;case 8:return s==Client::State::Closing&&f.link.serial==serial+2;default:return r.state()==RemoteVolume::State::Close&&s==Client::State::Ready;}};
  for(unsigned i=0;i<30000&&!reached();i++)tick(f,r);assert(reached());auto connections=f.link.serial;r.cancel();pump(f,r);
  assert(f.link.serial==connections);for(unsigned i=0;i<20;i++)tick(f,r);assert(f.link.serial==connections);
  if(phase==9){assert(r.state()==RemoteVolume::State::Done&&!r.uncertain()&&contents(f.volume.api(),"/target")==std::string(6000,'r'));}
  else {assert(r.state()==RemoteVolume::State::Failed&&r.error()[0]);if(phase<7)assert(contents(f.volume.api(),"/target")=="original");if(phase>=5)assert(r.uncertain());}
  assert(r.close());
 }
 // A vanished target after HEAD must not silently become a create.
 {Fixture f;AtomicLocal local;assert(f.request("PUT","/target",nullptr,"original")==201);RemoteVolume r(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});auto serial=f.link.serial;assert(r.upload(local.volume(),"/models/model.stl","/target",f.now,true));for(unsigned i=0;i<30000&&!(f.link.serial==serial+1&&f.client.state()==Client::State::Connect);i++)tick(f,r);assert(f.volume.api()->remove(f.volume.api()->context,"/target"));pump(f,r);assert(r.state()==RemoteVolume::State::Failed&&f.client.status()==412&&contents(f.volume.api(),"/target").empty());assert(r.close());}
 // Actual server write/sync failures leave the original intact. A failure
 // from replace_file may occur either side of publication, so both outcomes
 // are unknown to the client and neither is automatically retried/removed.
 for(unsigned mode=1;mode<=4;mode++){
  ReplacementServerFault server;Fixture f(server.base());AtomicLocal local;assert(f.request("PUT","/target",nullptr,"original")==201);server.mode=mode;
  RemoteVolume r(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});assert(r.upload(local.volume(),"/models/model.stl","/target",f.now,true));pump(f,r);
  assert(r.state()==RemoteVolume::State::Failed&&f.client.status()==(mode<3?507u:500u));assert(r.uncertain()==(mode>=3));
  assert(contents(server.volume.api(),"/target")== (mode==4?"readonly source":"original"));assert(server.replacements==unsigned(mode>=3));
  if(mode>=3)assert(strstr(r.error(),"unknown"));auto connections=f.link.serial;for(unsigned i=0;i<20;i++)tick(f,r);assert(f.link.serial==connections&&server.replacements==unsigned(mode>=3));assert(r.close());
 }
 // Transport failure during the PUT is uncertain, closes its source and does
 // not reconnect/replay. Failure before body transfer preserves old content.
 {Fixture f;AtomicLocal local;assert(f.request("PUT","/target",nullptr,"original")==201);RemoteVolume r(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});auto serial=f.link.serial;assert(r.upload(local.volume(),"/models/model.stl","/target",f.now,true));
  for(unsigned i=0;i<30000&&!(f.link.serial==serial+2&&f.client.state()==Client::State::Header&&!f.link.up.empty());i++)tick(f,r);
  assert(f.link.serial==serial+2);f.link.drop=true;pump(f,r);f.link.drop=false;
  assert(r.state()==RemoteVolume::State::Failed&&r.uncertain()&&strstr(r.error(),"unknown"));assert(contents(f.volume.api(),"/target")=="original"&&f.link.serial==serial+2);assert(r.close());
 }
 // Source unavailability during cancellation keeps the read handle until
 // it can close. Lost owner/retained socket custody prevents further calls.
 {Fixture f;AtomicLocal local;assert(f.request("PUT","/target",nullptr,"original")==201);RemoteVolume r(f.client,{&f,[](void*c){return static_cast<Fixture*>(c)->link.owner;}});auto serial=f.link.serial;assert(r.upload(local.volume(),"/models/model.stl","/target",f.now,true));for(unsigned i=0;i<30000&&!(f.link.serial==serial+1&&f.client.state()==Client::State::Connect);i++)tick(f,r);local.available=false;r.cancel();for(unsigned i=0;i<10;i++)tick(f,r);assert(r.state()==RemoteVolume::State::Close&&!r.close());local.available=true;pump(f,r);assert(r.state()==RemoteVolume::State::Failed&&contents(f.volume.api(),"/target")=="original"&&f.link.serial==serial+1);assert(r.close());}
 for(bool retainedClose:{false,true}){
  RawPeer f;AtomicLocal local;assert(f.remote.upload(local.volume(),"/models/model.stl","/target",f.now,true));
  for(unsigned i=0;i<10000&&f.client.state()!=Client::State::Reply;i++)f.tick();assert(f.client.state()==Client::State::Reply);
  if(retainedClose){f.link.down="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";f.link.accepted=true;for(unsigned i=0;i<10000&&f.client.state()!=Client::State::Closing;i++)f.tick();f.link.closeFault=true;}
  else f.link.owner=false;
  f.tick();assert(f.remote.state()==RemoteVolume::State::Retained);auto calls=f.link.calls;f.remote.cancel();assert(!f.remote.close());f.tick();assert(f.link.calls==calls);
  // The retained fixture is deliberately not cleaned by the runtime path.
  local.owned=false;
 }
 puts("Confirmed replacement: explicit generic SD wildcard, exact Watch revisions, default exclusive reset, rejected HEAD/tag, ten cancellation boundaries, late completion, vanished target, write/sync/unknown-publication and transport failures PASS");
}
