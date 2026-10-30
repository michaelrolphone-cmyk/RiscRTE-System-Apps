#include "Node.h"
#include "Digest.h"
namespace Infra {
bool Node::acceptSetup(){
 if(!parse(response,portal.input+portal.bodyAt,portal.bodySize)||!keys(response.as<JsonObjectConst>(),{"ssid","password","pairing"})||!text(response["ssid"],32)||!text(response["password"],63,true))return false;
 const size_t pn=strlen(response["password"]);if(pn&&pn<8)return false;
 auto pairing=response["pairing"].as<JsonObjectConst>();
 const bool existing=text(journal.state["participant_id"],128);
 if(setupRecovery||(existing&&!response["pairing"].isNull())){const char* ref=config["service"]["credential_ref"]|"";if(!text(pairing["credentials"][ref],1024)||strcmp(pairing["credentials"][ref],credential(ref)))return false;}

 // A pairing may initialize an empty installation. It cannot replace active
 // identity or unresolved execution custody during a Wi-Fi correction.
 if(!response["pairing"].isNull()&&!existing){
  if(!keys(pairing,{"format","policy","credentials","bootstrap","enrollment_id","credential_sha256"},{"participant_id","installation_id"})||pairing["format"]!="riscrte-setup/1"||text(journal.state["participant_id"],128)||journal.state["pending"].size()||!journal.state["job"].isNull()||measureJson(pairing["policy"])>PolicyMax||(!text(pairing["participant_id"],128)&&(!text(pairing["bootstrap"],1024)||!text(pairing["enrollment_id"],128)))||!digest(pairing["credential_sha256"])||!pairing["credentials"].is<JsonObjectConst>()||measureJson(pairing["credentials"])>3500)return false;
  for(auto kv:pairing["credentials"].as<JsonObjectConst>())if(!text(kv.value(),1024))return false;
  // Published implementation/contract pins and low-water bounds stay tied to
  // this exact image; a provisioning file supplies endpoints and authority.
  for(const char* k:{"contracts","providers"}){char a[65],b[65];canonicalDigest(config["service"][k],a);canonicalDigest(pairing["policy"]["service"][k],b);if(strcmp(a,b))return false;}
  char a[65],b[65];canonicalDigest(config["low_water"],a);canonicalDigest(pairing["policy"]["low_water"],b);if(strcmp(a,b))return false;
  request.set(pairing["policy"]);request["wifi"]["ssid"]=response["ssid"];if(!policy(request))return false;
  const char* ref=request["service"]["credential_ref"]|"";if(!text(pairing["credentials"][ref],1024))return false;
  dtls_sha256_data(reinterpret_cast<const uint8_t*>(pairing["credentials"][ref].as<const char*>()),strlen(pairing["credentials"][ref].as<const char*>()),a);if(pairing["credential_sha256"]!=a)return false;
  for(auto endpoint:request["endpoints"].as<JsonArrayConst>())for(const char* key:{"credential_ref","site_credential_ref"}){const char* ref=endpoint[key]|"";if(ref[0]&&!text(pairing["credentials"][ref],1024))return false;}
  const char* site=request["service"]["site_credential_ref"]|"";if(site[0]&&!text(pairing["credentials"][site],1024))return false;
  if(text(pairing["participant_id"],128)&&!text(pairing["installation_id"],64))return false;
  config.set(request);journal.state["credentials"]=pairing["credentials"];for(const char* k:{"bootstrap","enrollment_id","credential_sha256"})journal.state[k]=pairing[k];
  journal.state["configuration"]=config["configuration"];journal.state["configuration_revision"]=config["configuration"]["revision"];
  if(text(pairing["participant_id"],128)){journal.state["participant_id"]=pairing["participant_id"];journal.state["installation_id"]=pairing["installation_id"];}else journal.state["installation_id"]=portal.nonce;
 }else if(!credential(config["service"]["credential_ref"])[0]||(!text(journal.state["participant_id"],128)&&!text(journal.state["bootstrap"],1024)))return false;
 config["wifi"]["ssid"]=response["ssid"];journal.state["credentials"][config["wifi"]["credential_ref"]|""]=response["password"];
 journal.state["setup_policy"]=config;journal.state["setup_pending"]=true;
 return journal.valid()&&!config.overflowed();
}
int Node::setupStep(){
 if(drain){int r=portal.close();if(r==-2)return retained();if(r)return 1;if(journal.writeToken){r=journal.collect();if(r==-2)return retained();if(r)return 1;}r=station.close(station.context);if(r==-2)return retained();if(r)return 1;phase=Phase::Stopped;return 0;}
 if(phase==Phase::SetupStart){
  if(!station.access_point||!portal.initialize())return block(Phase::ProtocolBlocked,"setup_transport_unavailable");
  int r=station.access_point(station.context,"RiscRTE-Setup","RiscRTE-Setup");if(r==-2)return retained();if(r<0)return block(Phase::ProtocolBlocked,"setup_radio_failed");if(r)return 1;
  if(!portal.nonce[0]){uint8_t random[16];r=entropy.fill(entropy.context,random,sizeof(random));if(r==RISC_ENTROPY_RETAINED)return retained();if(r==RISC_ENTROPY_UNAVAILABLE||r==RISC_ENTROPY_BUSY)return 1;if(r)return block(Phase::ProtocolBlocked,"setup_entropy_failed");for(unsigned i=0;i<16;++i)snprintf(portal.nonce+2*i,3,"%02x",random[i]);Portal::wipe(random,sizeof(random));}
  r=portal.open();if(r==-2)return retained();if(r<0)return block(Phase::ProtocolBlocked,"setup_listener_failed");setupDeadline=now()+120000;reason(Phase::SetupPortal,"connect_phone_to_RiscRTE_Setup");return 1;
 }
 if(phase==Phase::SetupPortal){if(setupRecovery&&!portal.client&&now()>=setupDeadline){phase=Phase::SetupLeave;return 1;}int r=portal.tick(now());if(r==-2)return retained();if(r==3){if(!acceptSetup()){portal.submitted=false;portal.respond(400,"Check the Wi-Fi details and choose the private pairing file for this firmware. Existing settings were not changed.");response.clear();request.clear();return 1;}response.clear();request.clear();saveSequence=journal.seq+1;phase=Phase::SetupSave;}return 1;}
 if(phase==Phase::SetupSave){
  int r=journal.writeToken?journal.collect():journal.reload?journal.load():journal.save();if(r==-2)return retained();if(r>0)return 1;
  setupFailed=r<0||journal.seq!=saveSequence||journal.loadedCrc!=journal.desiredCrc;
  portal.respond(setupFailed?500:200,setupFailed?"Settings could not be committed. Power-cycle the device and try setup again.":"Settings saved. The setup Wi-Fi will turn off while the device connects. Reconnect your phone to your normal Wi-Fi. If RiscRTE-Setup returns, reconnect to it to correct the Wi-Fi details.");
  setupDeadline=now()+5000;phase=Phase::SetupFinish;return 1;
 }
 if(phase==Phase::SetupFinish){int r=portal.tick(now());if(r==-2)return retained();if(r==2||now()>=setupDeadline)phase=Phase::SetupLeave;return 1;}
 if(phase==Phase::SetupLeave||phase==Phase::SetupRetry){int r=portal.close();if(r==-2)return retained();if(r)return 1;r=station.close(station.context);if(r==-2)return retained();if(r)return 1;
  if(setupFailed)return block(Phase::StorageBlocked,"setup_commit_failed");
  if(phase==Phase::SetupRetry){portal.nonce[0]=0;portal.message=setupRecovery?"Wi-Fi connection failed. Choose your private pairing file to change Wi-Fi settings.":"Wi-Fi connection failed. Check the network name and password, then try again.";reason(Phase::SetupStart,"setup_wifi_retry");}
  else{setupDeadline=0;reason(Phase::SetupJoin,"setup_joining_wifi");}return 1;
 }
 if(phase==Phase::SetupJoin){if(!setupDeadline)setupDeadline=now()+30000;int r=station.connect(station.context,config["wifi"]["ssid"],credential(config["wifi"]["credential_ref"]));if(r==-2)return retained();if(r<0||now()>=setupDeadline){reason(Phase::SetupRetry,"setup_wifi_failed");return 1;}if(r)return 1;
  journal.state["setup_pending"]=false;configureCadence();loaded=true;portal.dispose();sessionId[0]=boot[0]=0;rebootRecovery=true;snapshotNeeded=inventoryNeeded=true;snapshotStage=0;snapshotAfter[0]=0;return persist(Phase::Nonce);
 }
 return 1;
}
}
