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
 // Both uncertain commit outcomes: only a proven complete snapshot earns Saved.
 for(bool applied:{false,true}){initial(seed);Node u;initialize(u);step(u,2);form["pairing"]=savedPairing;form["pairing"]["policy"]=policy;request(post(u,form));commit=applied;outcome=RISC_APP_DATA_COMMIT_UNKNOWN;for(unsigned i=0;i<400&&u.phase!=Phase::SetupFinish;++i)step(u);assert(u.phase==Phase::SetupFinish&&u.setupFailed==!applied);assert(strstr(u.portal.body,applied?"Settings saved.":"could not be committed"));stop(u);}
 puts("Portal: captive redirect, page/session, CSRF and framing, pin validation, save-before-disconnect, wrong Wi-Fi retry, durable reboot, DNS bounds, uncertain commits PASS");
}
