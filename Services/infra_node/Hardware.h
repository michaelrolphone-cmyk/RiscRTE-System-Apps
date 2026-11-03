#pragma once
#include "Policy.h"
namespace Infra {
// Product-owned physical I/O. No pin numbers or arbitrary UART controllers
// cross the remote interface. The stock camera product leaves this unbound.
struct Hardware {
 void* context=nullptr;
 bool (*ready)(void*)=nullptr;
 int (*execute)(void*,const char*,JsonObjectConst,JsonObject)=nullptr;
};
inline bool hardwareCapability(const char* c){return !strcmp(c,"relay.bank")||!strcmp(c,"serial.rs485")||!strcmp(c,"sound.buzzer")||!strcmp(c,"light.rgb");}
inline const char* hardwareEffect(const char* c){return !strcmp(c,"serial.rs485")?"nonrepeatable_or_unobservable":!strcmp(c,"sound.buzzer")?"observable_effect":"convergent_state";}
inline bool hardwareInput(const char* c,const char* op,JsonObjectConst in){
 if(!strcmp(c,"relay.bank"))return !strcmp(op,"set")&&keys(in,{"channel","on"})&&integer(in["channel"],1,6)&&in["on"].is<bool>();
 if(!strcmp(c,"light.rgb"))return !strcmp(op,"set")&&keys(in,{"red","green","blue"})&&integer(in["red"],0,255)&&integer(in["green"],0,255)&&integer(in["blue"],0,255);
 if(!strcmp(c,"sound.buzzer"))return !strcmp(op,"tone")&&keys(in,{"frequency_hz","duration_ms"})&&integer(in["frequency_hz"],100,10000)&&integer(in["duration_ms"],1,5000);
 if(strcmp(c,"serial.rs485")||strcmp(op,"exchange")||!keys(in,{"baud","parity","stop_bits","tx_hex","rx_max","timeout_ms"})||!integer(in["baud"],1200,115200)||!integer(in["stop_bits"],1,2)||!integer(in["rx_max"],0,256)||!integer(in["timeout_ms"],10,2000)||!text(in["tx_hex"],512)||strlen(in["tx_hex"]|"")%2)return false;
 if(in["parity"]!="none"&&in["parity"]!="even"&&in["parity"]!="odd")return false;
 for(const char* p=in["tx_hex"];*p;++p)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')))return false;
 return true;
}
}
