#pragma once
#include "Node.h"
#ifdef NDEBUG
#error Relay tests require assertions
#endif
#include <cassert>
#include <fstream>
#include <string>
#include <map>
#include <memory>
using namespace Infra;
static uint64_t ticks=1000;static std::string sent,url;static std::map<std::string,std::string> headers;static unsigned posts=0;
static uint64_t clockNow(void*){return ticks;}
static int openHttp(void*,const risc_http_request_v2* r,uint64_t* token){assert(r->method==RISC_HTTP_POST);url=r->url;sent.assign(static_cast<const char*>(r->body),r->body_bytes);headers.clear();for(unsigned i=0;i<r->header_count;++i)headers[r->headers[i].name]=r->headers[i].value;*token=1;if(url=="https://hooks.example/notify")++posts;return 0;}
static void saved(Node& n){assert(n.phase==Phase::Save);assert(n.journal.valid());assert(measureJson(n.journal.state)<StateMax-32);n.phase=n.afterSave;n.journal.seq=n.saveSequence;}
static void reply(Node& n,const char* body,unsigned code=200){assert(n.phase==Phase::Send);assert(n.transmit()==1);assert(n.phase==Phase::Receive);n.net.token=0;n.net.outcome=0;n.net.info.status_code=code;n.net.size=strlen(body);memcpy(n.net.body,body,n.net.size+1);assert(n.received()>=0);}
static void init(Node& n,JsonDocument& cfg){assert(n.initialize());n.config.set(cfg);n.clock={1,sizeof(n.clock),nullptr,clockNow};n.net.utc=1793400000;n.net.utcAt=ticks;n.net.api.open=openHttp;n.phase=Phase::Ready;assert(!deserializeJson(n.journal.state,R"({"schema_version":1,"installation_id":"test","credential_generation":1,"credentials":{"relay-token":"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","hook-token":"header-secret","hook-sign":"signing-secret"},"participant_id":"test","cursor":0,"pending":[],"job":null,"checkpoints":{},"configuration_revision":1})"));strcpy(n.boot,"test-boot");n.entropy.fill=[](void*,void* out,uint32_t size)->int32_t{memset(out,7,size);return 0;};}
static void receive(Node& n,const char* event){n.relayHealthAt=ticks+180000;n.relayRetryAt=0;for(auto& at:n.relayPollAt)at=ticks+30000;n.relayPollAt[0]=0;assert(n.relayStep()==1&&n.action==Action::RelayPoll);reply(n,event);saved(n);}
static std::unique_ptr<Node> reboot(Node& old,JsonDocument& cfg){std::string disk;serializeJson(old.journal.state,disk);auto n=std::make_unique<Node>();init(*n,cfg);assert(parse(n->journal.state,disk.data(),disk.size())&&n->journal.valid());n->relayReady=true;n->relayIdentityReady=true;n->relayHealthAt=ticks+180000;old.phase=Phase::Stopped;old.dispose();return n;}
