#pragma once
#include <RiscAppDataExportV1.h>
#include "WebDavProperties.h"
#include <cstddef>
#include <cstdint>

namespace RiscWebDav {
constexpr size_t HeaderMax=4096, PropertyBodyMax=4096, CatalogMax=16;
struct Request {
 char method[16]{},path[RISC_APP_DATA_EXPORT_PATH_MAX+1]{},host[128]{};
 char authorization[1537]{},ifMatch[96]{},ifNoneMatch[96]{};
 char requestTarget[769]{};
 uint32_t contentLength=0;
 uint8_t depth=2; // Omitted Depth means infinity, never silently Depth:1.
 bool expectContinue=false;
 char contentType[128]{};
};
/* Decode a complete, bounded HTTP/1.1 header, including its final CRLFCRLF.
 * The caller owns framing/timeouts. No filesystem or network call occurs here.
 * Returns zero on success, otherwise the HTTP status to send before closing. */
unsigned parseRequest(const char*,size_t,Request&);

enum class AuthPhase { Headers, Complete };
struct Hooks {
 void* context=nullptr;
 bool (*owner)(void*)=nullptr;
 bool (*authorize)(void*,const Request&,AuthPhase,const void*,size_t)=nullptr;
 /* Required, copied at begin; must be a single safe HTTP challenge value. */
 const char* authenticationChallenge=nullptr;
 void* (*allocate)(size_t)=nullptr;
 void (*deallocate)(void*)=nullptr;
};
enum class State { Idle,Catalog,Prepare,Properties,Ready,Retained,Done };
/* One foreground-owned request. The caller retains its export grant and input
 * body through completion. This object never owns a socket or borrowed provider
 * grant. A retained/context-lost backend outcome forbids any output/cleanup.
 * step performs at most one filesystem operation; policy enumeration is copied
 * one entry at a time. No filesystem handle survives a step. */
class Transaction final {
 public:
 Transaction()=default;
 Transaction(const Transaction&)=delete;
 Transaction& operator=(const Transaction&)=delete;
 bool begin(const risc_app_data_export_v1*,const Hooks&,const Request&,const void*,size_t);
 State step();
 State state()const{return state_;}
 unsigned status()const{return status_;}
 const char* header()const{return state_==State::Ready?header_:nullptr;}
 size_t headerSize()const{return state_==State::Ready?headerSize_:0;}
 const void* body()const{return state_==State::Ready?output_:nullptr;}
 size_t bodySize()const{return state_==State::Ready&&!head_?outputSize_:0;}
 /* Releases only owned memory after a clean request. False means retained;
  * caller must keep the grant and all associated transport custody. */
 bool finish();
 private:
 const risc_app_data_export_v1* files_=nullptr;
 Hooks hooks_{};Request request_{};
 const void* input_=nullptr;size_t inputSize_=0;
 risc_app_data_export_entry_v1 catalog_[CatalogMax]{};
 unsigned count_=0,row_=0,rows_=0;
 struct Row {char path[RISC_APP_DATA_EXPORT_PATH_MAX+1]{};bool directory=false;} listing_[CatalogMax+1]{};
 State state_=State::Idle;
 unsigned status_=0;
 char header_[768]{},etag_[32]{},challenge_[256]{};
 size_t headerSize_=0,outputSize_=0,outputCapacity_=0;
 unsigned char* output_=nullptr;
 bool head_=false;
 PropertyRequest properties_{};
 uint32_t fileSize_=0;uint64_t revision_=0;
 enum class Operation { None,Read,Write,Properties } operation_=Operation::None;
 bool valid();
 bool reserve(size_t);
 bool append(const char*);
 bool escaped(const char*,bool uri=false);
 bool propertyRow(const Row&,uint32_t,uint64_t);
 bool selected(const char*,bool*)const;
 bool directory(const char*)const;
 bool buildListing();
 void response(unsigned,const char* type="text/plain; charset=utf-8",size_t representationSize=SIZE_MAX);
 void error(unsigned,const char*);
 bool backend(int32_t);
};
}
