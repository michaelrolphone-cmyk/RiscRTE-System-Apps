#pragma once
#include "../../../Services/infra_node/Hardware.h"
#include <RiscHttpClientV2.h>

namespace BiomeRelay {
// The product owns one Client for its lifetime. All methods except the
// internal one-shot RSA key-generation task run on the native HTTP owner task.
class Client {
 struct Implementation;
 Implementation* impl=nullptr;
 public:
 Client()=default;
 Client(const Client&)=delete;
 Client& operator=(const Client&)=delete;
 bool begin(const uint8_t mac[6],const risc_http_client_v2& http,
            Infra::Hardware hardware,uint64_t (*clock)(void*),void* clockContext=nullptr);
 // Always call tick to pump asynchronous outputs. mayOpenHttp only grants a
 // NEW request; a held request continues until its native cleanup completes.
 void tick(bool networkConnected,bool mayOpenHttp=true);
 bool busy()const;
 bool retained()const;
 bool effectsBusy()const;
 bool outputsActive()const;
 bool configured()const;
 bool ready()const;
 bool enrolled()const;
 bool acceptConfig(JsonObjectConst config);
 void publicIdentity(JsonObject out)const;
};
}
