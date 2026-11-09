#include "Node.h"
#include "Digest.h"
#ifdef NDEBUG
#error Portal integration tests require active assertions after product headers
#endif
#include <cassert>
#include <fstream>
#include <string>
#include <vector>
using namespace Infra;
static std::string wire,output,policyBytes;static size_t at;static bool accepted=false,ap=false,connected=false,commit=true;static int outcome=0;static uint64_t tick=10,revision=1;static std::vector<uint8_t> disk,pending;
static int listen(void*,const risc_tcp_listen_v1* r,uint64_t* h){assert(ap&&r->port==80&&r->address[3]==1);*h=1;return 0;}
static int accept(void*,uint64_t,uint64_t* h){if(accepted||wire.empty())return 1;accepted=true;*h=2;return 0;}
static int read(void*,uint64_t,void* p,uint32_t n,uint32_t* got){*got=0;if(at==wire.size())return 1;*got=std::min<size_t>(std::min<unsigned>(n,79),wire.size()-at);memcpy(p,wire.data()+at,*got);at+=*got;return 0;}
static int write(void*,uint64_t,const void* p,uint32_t n,uint32_t* sent){*sent=std::min<unsigned>(n,93);output.append(static_cast<const char*>(p),*sent);return 0;}
static int close(void*,uint64_t){return 0;}
static int bind(void*,const risc_tcp_listen_v1* r,uint64_t* h){assert(ap&&r->port==53);*h=3;return 0;}
static int receive(void*,uint64_t,void*,uint32_t,uint32_t* n,risc_tcp_listen_v1*){*n=0;return 1;}
static int send(void*,uint64_t,const void*,uint32_t,const risc_tcp_listen_v1*){return 0;}
static risc_tcp_listener_v1 sockets{1,sizeof(sockets),nullptr,listen,accept,read,write,close,0,nullptr,nullptr,RISC_DATAGRAM_TAG,bind,receive,send};
static int stat(void*,const char* name,uint32_t* size,uint64_t* rev){*size=!strcmp(name,"node-policy.json")?policyBytes.size():disk.size();*rev=revision;return 0;}
static int storedRead(void*,const char* name,uint64_t expected,void* p,uint32_t cap,uint32_t* size,uint64_t* rev){if(cap>RISC_APP_DATA_FILE_MAX)return RISC_APP_DATA_INVALID;assert(expected==revision);*size=!strcmp(name,"node-policy.json")?policyBytes.size():disk.size();assert(*size<=cap);memcpy(p,!strcmp(name,"node-policy.json")?static_cast<const void*>(policyBytes.data()):disk.data(),*size);*rev=revision;return 0;}
static int begin(void*,const char*,uint64_t expected,const void* p,uint32_t n,uint64_t* h){assert(expected==revision);pending.assign(static_cast<const uint8_t*>(p),static_cast<const uint8_t*>(p)+n);*h=9;return 0;}
static int collect(void*,uint64_t h,int32_t* result){assert(h==9);if(commit){disk=pending;++revision;}*result=outcome;return 0;}
static risc_bound_app_data_async_v1 storage{{1,sizeof(storage),nullptr,stat,storedRead,nullptr},RISC_BOUND_APP_DATA_ASYNC_TAG,1,begin,collect};
static int join(void*,const char*,const char*){assert(!ap);return connected?0:-1;}static int poll(void*){return 0;}static int radioClose(void*){ap=false;return 0;}static int startAP(void*,const char* ssid,const char* pass){assert(!strcmp(ssid,"RiscRTE-Setup")&&strlen(pass)>=8);ap=true;return 0;}static uint64_t now(void*){return tick;}static int entropy(void*,void* p,uint32_t n){assert(ap||connected);memset(p,42,n);return 0;}
static void initialize(Node& n){assert(n.journal.initialize(&storage.base));n.portal.api=sockets;n.station={1,sizeof(n.station),nullptr,join,poll,radioClose,startAP};n.clock={1,sizeof(n.clock),nullptr,now};n.entropy={1,sizeof(n.entropy),nullptr,entropy};}
static void initial(const char* state){disk.resize(StateMax);disk.resize(frame(disk.data(),disk.size(),state,strlen(state),1));revision=1;commit=true;outcome=0;ap=connected=false;wire.clear();output.clear();}
static void step(Node& n,unsigned count=1){while(count--){assert(n.advance()>=0);tick+=10;}}
static void request(std::string v){wire=v;at=0;accepted=false;output.clear();}
static std::string post(Node& n,JsonDocument& data,std::string extra=""){std::string body;serializeJson(data,body);return "POST /setup HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/json\r\nX-Setup-Token: "+std::string(n.portal.nonce)+"\r\nContent-Length: "+std::to_string(body.size())+"\r\n"+extra+"\r\n"+body;}
static void stop(Node& n){n.drain=true;for(unsigned i=0;i<20&&n.phase!=Phase::Stopped;++i)step(n);assert(n.phase==Phase::Stopped);n.dispose();}

static unsigned nativeCommits=0;
static void onNativeCommit(JsonObjectConst config){
 ++nativeCommits;assert(biomeSetup(config));
 const char* payload=nullptr;size_t size=0;uint64_t seq=0;
 assert(unframe(disk.data(),disk.size(),payload,size,seq)&&seq>1);
 JsonDocument saved;assert(parseJournal(saved,payload,size));
 assert(sameBiomeSetup(config,saved["biomerte"]));
}
static void publicNativeIdentity(JsonObject out){
 out["fingerprint"]="public-fingerprint";
 out["public_key"]="-----BEGIN PUBLIC KEY-----\nexample\n-----END PUBLIC KEY-----\n";
 out["pairing"]="awaiting_approval";out["error"]="display \"quoted\" text";
}
static void nativeForm(JsonDocument& form){
 form.clear();form["ssid"]="Home";form["password"]="native-password";
 form["pairing"]["format"]="biomerte-relay-setup/1";
 form["pairing"]["biomerte"]["url"]="https://controller.test/api/provider/call";
 form["pairing"]["biomerte"]["realm"]="site";
}
static bool applyForm(Node& n,JsonDocument& form){
 std::string body;serializeJson(form,body);assert(body.size()<=Portal::InputMax);
 memcpy(n.portal.input,body.data(),body.size());n.portal.bodyAt=0;n.portal.bodySize=body.size();
 return n.acceptSetup();
}

int main(int argc,char** argv){assert(argc==3);std::ifstream file(argv[1]);policyBytes={std::istreambuf_iterator<char>(file),{}};
 const char* seed=R"({"schema_version":1,"installation_id":"example-installation","credential_generation":1,"credentials":{},"participant_id":"","cursor":0,"pending":[],"job":null,"checkpoints":{},"configuration_revision":1})";
 // Request-line recognition must match actual browser HTTP, including the
 // complete version suffix, rather than a hand-counted prefix length.
 for(const char* version:{"HTTP/1.0","HTTP/1.1"}){
  Portal parser;assert(parser.initialize()==false); // no listener API needed
  strcpy(parser.nonce,"0123456789abcdef0123456789abcdef");
  std::string header=std::string("POST /setup ")+version+"\r\nHost: 192.168.4.1\r\nContent-Type: application/json\r\nX-Setup-Token: "+parser.nonce+"\r\nContent-Length: 2\r\n\r\n{}";
  memcpy(parser.input,header.data(),header.size());parser.used=header.size();parser.decodeHeaders();assert(parser.headers&&!parser.answered&&parser.bodySize==2);parser.dispose();
 }
 for(const char* line:{"POST /setup HTTP/1.","POST /setup HTTP/1.10","POST /setupX HTTP/1.1","POST /setup HTTP/2.0"}){
  Portal parser;parser.initialize();std::string raw=std::string(line)+"\r\nHost: 192.168.4.1\r\n\r\n";memcpy(parser.input,raw.data(),raw.size());parser.used=raw.size();parser.decodeHeaders();assert(parser.answered&&strstr(parser.header,"405 Error"));parser.dispose();
 }
 initial(seed);Node n;initialize(n);step(n,2);assert(n.phase==Phase::SetupPortal&&ap&&n.portal.dns&&n.portal.listener);
 request("GET /generate_204 HTTP/1.1\r\nHost: connectivitycheck.gstatic.com\r\n\r\n");step(n,30);assert(output.find("302 Found")!=std::string::npos&&output.find("Location: http://192.168.4.1/")!=std::string::npos);
 request("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");step(n,100);assert(output.find("Set up your RiscRTE")!=std::string::npos&&output.find("Content-Length:")!=std::string::npos);
 request("GET /session HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");step(n,30);assert(output.find(n.portal.nonce)!=std::string::npos);
 JsonDocument form;form["ssid"]="Home";form["password"]="secret-password";form["pairing"]=nullptr;
 request(post(n,form,"Origin: http://evil.example\r\n"));step(n,30);assert(output.find("403 Error")!=std::string::npos&&n.journal.writes==0);
 request(post(n,form,"Content-Length: 1\r\n"));step(n,30);assert(output.find("400 Error")!=std::string::npos&&n.journal.writes==0);
 request(post(n,form));step(n,50);assert(output.find("400 Error")!=std::string::npos&&n.journal.writes==0);
 JsonDocument policy;assert(parse(policy,policyBytes.data(),policyBytes.size()));auto p=form["pairing"].to<JsonObject>();p["format"]="riscrte-setup/1";p["policy"]=policy;p["bootstrap"]="bootstrap";p["enrollment_id"]="enrollment";p["credentials"]["participant"]="participant-secret";char hash[65];dtls_sha256_data(reinterpret_cast<const uint8_t*>("participant-secret"),18,hash);p["credential_sha256"]=hash;
 for(auto e:policy["endpoints"].as<JsonArrayConst>())p["credentials"][e["credential_ref"]|""]="test-secret";
 p["policy"]["service"]["providers"][0]["package_digest"]=std::string(64,'f');request(post(n,form));step(n,200);assert(output.find("400 Error")!=std::string::npos&&n.journal.writes==0);
 p["policy"]=policy;request(post(n,form));for(unsigned i=0;i<400&&n.phase!=Phase::SetupSave;++i)step(n);assert(n.phase==Phase::SetupSave&&output.empty()&&ap);step(n);assert(n.journal.writeToken&&ap&&output.empty());step(n);assert(n.phase==Phase::SetupFinish&&n.journal.state["setup_pending"]==true);
 for(unsigned i=0;i<100&&n.phase!=Phase::SetupJoin;++i)step(n);assert(n.phase==Phase::SetupJoin&&!ap&&output.find("Settings saved.")!=std::string::npos);step(n,3);assert(n.phase==Phase::SetupPortal&&ap);assert(n.portal.message[0]=='W');
 // Correction needs no service secret replacement and persists across reboot.
 JsonDocument savedPairing;savedPairing.set(p);form["pairing"]=nullptr;form["password"]="correct-password";request(post(n,form));connected=true;for(unsigned i=0;i<400&&n.phase!=Phase::Nonce;++i)step(n);assert(n.phase==Phase::Nonce&&!ap&&!n.portal.listener&&!n.portal.dns&&!n.portal.client&&n.journal.state["setup_pending"]==false);
 n.phase=Phase::Stopped;n.dispose();Node reboot;initialize(reboot);step(reboot);assert(reboot.phase==Phase::Connect&&reboot.config["wifi"]["ssid"]=="Home"&&!strcmp(reboot.credential("wifi"),"correct-password"));reboot.phase=Phase::Stopped;reboot.dispose();
 // DNS: A gets local address, AAAA gets an empty answer, compression/truncation rejected.
 uint8_t query[512]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,1,0,1};assert(Portal::dnsReply(query,19)==35&&query[7]==1&&query[34]==1);
 uint8_t aaaa[512]={0,1,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,28,0,1};assert(Portal::dnsReply(aaaa,19)==19&&aaaa[7]==0);aaaa[2]=1;aaaa[12]=0xc0;assert(!Portal::dnsReply(aaaa,19));
 // An exact published v1.2.2 pairing cohort is upgraded to this image's
 // implementation pins. A modified/mixed older cohort must still fail closed.
 std::ifstream legacyFile(argv[2]);std::string legacyBytes{std::istreambuf_iterator<char>(legacyFile),{}};JsonDocument legacy;assert(parse(legacy,legacyBytes.data(),legacyBytes.size())==false);assert(!deserializeJson(legacy,legacyBytes));
 for(bool tampered:{true,false}){initial(seed);Node u;initialize(u);step(u,2);form["pairing"]=savedPairing;form["pairing"]["policy"]=policy;form["pairing"]["policy"]["service"]["providers"]=legacy;
  if(tampered)form["pairing"]["policy"]["service"]["providers"][0]["revision"]=1;
  request(post(u,form));for(unsigned i=0;i<400&&u.phase!=Phase::SetupSave;++i)step(u);
  if(tampered)assert(u.phase==Phase::SetupPortal&&u.journal.writes==0&&output.find("400 Error")!=std::string::npos);
  else{assert(u.phase==Phase::SetupSave);char expected[65],actual[65];canonicalDigest(policy["service"]["providers"],expected);canonicalDigest(u.config["service"]["providers"],actual);assert(!strcmp(expected,actual));}
  stop(u);
 }
 // Initial setup accepts a bearer-only webhook. Every declared secret must resolve.
 for(bool missing:{true,false}){
  initial(seed);Node u;initialize(u);step(u,2);form["pairing"]=savedPairing;
  auto pair=form["pairing"].as<JsonObject>();
  auto relay=pair["policy"]["relay"].to<JsonObject>();relay["schema_version"]=1;
  relay["relay_id"]="test-relay";relay["url"]="https://bus.test/relay";relay["credential_ref"]="participant";relay["site_credential_ref"]="";
  relay["allowed_origins"].add("https://receiver.test");relay["webhook_credential_refs"].add("callback");
  auto sub=relay["subscriptions"].add<JsonObject>();sub["id"]="callback-1";sub["channel"]="relay.diagnostics";
  sub["url"]="https://receiver.test/receipt";sub["enabled"]=true;sub["match"]["types"].add("relay.challenge");sub["match"]["filters"].to<JsonArray>();
  sub["auth"]["header"]="Authorization";sub["auth"]["credential_ref"]="callback";
  if(!missing)pair["credentials"]["callback"]="test-callback-token";
  request(post(u,form));for(unsigned i=0;i<400&&u.phase!=Phase::SetupSave;++i)step(u);
  if(missing)assert(u.phase==Phase::SetupPortal&&u.journal.writes==0&&output.find("400 Error")!=std::string::npos);
  else assert(u.phase==Phase::SetupSave&&u.config["relay"]["subscriptions"].size()==1);
  stop(u);
 }
 // Both uncertain commit outcomes: only a proven complete snapshot earns Saved.
 for(bool applied:{false,true}){initial(seed);Node u;initialize(u);step(u,2);form["pairing"]=savedPairing;form["pairing"]["policy"]=policy;request(post(u,form));commit=applied;outcome=RISC_APP_DATA_COMMIT_UNKNOWN;for(unsigned i=0;i<400&&u.phase!=Phase::SetupFinish;++i)step(u);assert(u.phase==Phase::SetupFinish&&u.setupFailed==!applied);assert(strstr(u.portal.body,applied?"Settings saved.":"could not be committed"));stop(u);}

 // The board opt-in admits a public native setup envelope without an old
 // Channel Bus secret; all other products retain their original first setup.
 for(bool enabled:{false,true}){
  initial(seed);Node u;initialize(u);u.allowBiomeSetup=enabled;step(u,2);nativeForm(form);
  assert(applyForm(u,form)==enabled&&u.journal.writes==0);
  if(enabled){assert(biomeSetup(u.journal.state["biomerte"]));assert(!u.credential("participant")[0]);}
  else assert(u.journal.state["biomerte"].isNull());
  stop(u);
 }
 // Staged configuration contains only public routing fields with a strict
 // HTTPS ingress route. No owner token or private key is accepted or persisted.
 JsonDocument native;nativeForm(form);native.set(form["pairing"]["biomerte"]);
 assert(biomeSetup(native));native["site"]="f45540bf9910412680734d0613e84460";assert(biomeSetup(native));native["site"]="";assert(biomeSetup(native));
 for(const char* url:{"http://controller.test/api/provider/call","https:///api/provider/call","https://user@controller.test/api/provider/call","https://controller.test/api/provider/call?token=secret","https://controller.test/other"}){
  JsonDocument bad;bad.set(native);bad["url"]=url;assert(!biomeSetup(bad));
 }
 for(const char* name:{"credential","owner_token","private_key"}){
  JsonDocument bad;bad.set(native);bad[name]="secret";assert(!biomeSetup(bad));
 }
 {JsonDocument bad;bad.set(native);bad["site"]="bad site";assert(!biomeSetup(bad));bad.remove("site");bad["resource"]="with space";assert(!biomeSetup(bad));bad["resource"]=std::string(129,'r');assert(!biomeSetup(bad));bad.remove("resource");bad["realm"]="owner";assert(!biomeSetup(bad));}
 {JsonDocument equal;equal.set(native);equal.remove("realm");assert(sameBiomeSetup(equal,native));}
 // Journal validation rejects any corrupt or secret-bearing native extension.
 {Journal j;assert(parse(j.state,seed,strlen(seed)));j.state["biomerte"]=native;assert(j.valid());j.state["biomerte"]["private_key"]="secret";assert(!j.valid());}
 // Existing legacy authority is still required when adding native routing.
 // A later legacy pairing with no native section preserves that routing.
 for(bool authenticated:{false,true}){
  initial(seed);Node u;initialize(u);u.allowBiomeSetup=true;step(u,2);
  u.journal.state["participant_id"]="existing-participant";
  u.journal.state["credentials"]["participant"]="participant-secret";
  form["pairing"]=savedPairing;form["pairing"]["biomerte"]=native;
  if(!authenticated)form["pairing"]["credentials"]["participant"]="wrong-secret";
  assert(applyForm(u,form)==authenticated);
  if(authenticated){
   assert(sameBiomeSetup(u.journal.state["biomerte"],native));
   form["pairing"]=savedPairing;assert(applyForm(u,form));
   assert(sameBiomeSetup(u.journal.state["biomerte"],native));
   u.setupRecovery=true;form["pairing"]=nullptr;assert(!applyForm(u,form));
  }else assert(u.journal.state["biomerte"].isNull());
  stop(u);
 }
 // First legacy setup may opt into native routing without changing legacy
 // credential or implementation-pin validation.
 {initial(seed);Node u;initialize(u);u.allowBiomeSetup=true;step(u,2);
  form["pairing"]=savedPairing;form["pairing"]["biomerte"]=native;
  assert(applyForm(u,form));assert(!strcmp(u.credential("participant"),"participant-secret"));
  assert(sameBiomeSetup(u.journal.state["biomerte"],native));stop(u);
 }
 // Native enrollment locks routing. The same envelope remains idempotent,
 // and a physically entered recovery session can correct only Wi-Fi.
 {initial(seed);Node u;initialize(u);u.allowBiomeSetup=true;step(u,2);
  u.journal.state["biomerte"]=native;u.biomeTargetLocked=true;nativeForm(form);
  assert(applyForm(u,form));form["pairing"]["biomerte"]["url"]="https://other.test/api/provider/call";
  assert(!applyForm(u,form)&&sameBiomeSetup(u.journal.state["biomerte"],native));
  u.setupRecovery=true;form["pairing"]=nullptr;form["password"]="corrected-native-password";
  assert(applyForm(u,form)&&sameBiomeSetup(u.journal.state["biomerte"],native));
  stop(u);
 }
 // An incomplete legacy installation or unresolved old execution cannot be
 // converted to a native-only installation by replacing its setup envelope.
 for(unsigned custody=0;custody<4;++custody){
  initial(seed);Node u;initialize(u);u.allowBiomeSetup=true;step(u,2);nativeForm(form);
  if(custody==0)u.journal.state["credentials"]["participant"]="old-secret";
  if(custody==1)u.journal.state["participant_id"]="old-participant";
  if(custody==2)u.journal.state["pending"].add<JsonObject>()["kind"]="unresolved";
  if(custody==3)u.journal.state["job"].to<JsonObject>()["kind"]="unresolved";
  assert(!applyForm(u,form)&&u.journal.state["biomerte"].isNull());stop(u);
 }
 // Native configuration is applied only after the exact new journal snapshot
 // is proven durable, including an ambiguous write resolved by reloading.
 for(unsigned result=0;result<3;++result){
  initial(seed);Node u;initialize(u);u.allowBiomeSetup=true;u.setupCommitted=onNativeCommit;
  step(u,2);nativeForm(form);nativeCommits=0;request(post(u,form));
  for(unsigned i=0;i<400&&u.phase!=Phase::SetupSave;++i)step(u);
  assert(u.phase==Phase::SetupSave&&nativeCommits==0);
  commit=result!=0;outcome=result<2?RISC_APP_DATA_COMMIT_UNKNOWN:RISC_APP_DATA_OK;
  step(u);assert(u.journal.writeToken&&nativeCommits==0);
  for(unsigned i=0;i<20&&u.phase!=Phase::SetupFinish;++i)step(u);
  assert(u.phase==Phase::SetupFinish&&u.setupFailed==(result==0));
  assert(nativeCommits==(result?1u:0u));stop(u);
 }
 // The optional public identity is JSON encoded; quotes and PEM newlines do
 // not corrupt the token/session response. No native fields on other boards.
 {Portal p;strcpy(p.nonce,"0123456789abcdef0123456789abcdef");p.message="Connect \"Home\".";
  p.sessionResponse();JsonDocument session;assert(parse(session,p.session,strlen(p.session)));
  assert(session["biomerte"].isNull());p.publicIdentity=publicNativeIdentity;p.sessionResponse();
  assert(parse(session,p.session,strlen(p.session))&&session["token"]==p.nonce);
  assert(session["biomerte"]["pairing"]=="awaiting_approval");
  assert(session["biomerte"]["error"]=="display \"quoted\" text");
  assert(!strstr(p.session,"PRIVATE KEY"));
 }

 puts("Portal: legacy security/regressions, native board opt-in and route validation, preserved authority, target locking, durable commit callbacks and public identity PASS");
}
