#include "bootstrap/Runtime.h"
#include "diagnostics/Checkpoint.h"
#include "RuntimeFileFixture.h"
#include <cassert>
#include <fstream>
#include <string>
using namespace RiscBoot;
namespace {
std::string mode;unsigned writes=0,stages=0;RiscDiagnostics::Checkpoint record{};Runtime* volatile held=nullptr;
bool owns(){return true;}bool health(risc_runtime_health_v1*){return true;}void delay(uint32_t){}bool log(const char*){return true;}
int32_t copy(const char* name,uint64_t token,const char* text,uint32_t size){++writes;return RiscDiagnostics::captureCheckpoint(record,1,2,name,token,text,size);}
}
extern "C" int last_attempt_test_stage(unsigned stage){
 ++stages;(void)stage;
 if(mode=="off")assert(writes==0);
 else {assert(writes==1);assert(std::string(record.application)=="default.elf");assert(std::string(record.text)=="last-attempt=fixture.operation");}
 return mode=="retained";
}
int main(int argc,char**argv){
 assert(argc==3);std::string root=argv[1];mode=argv[2];
 std::ofstream(root+"/board.json")<<R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"last-attempt","revision":"unspecified","buses":[],"devices":[]})";
 std::ofstream(root+"/boot.json")<<R"({"board":"board.json","default_app":"default.elf","drivers":[],"app_capabilities":[]})";
 Port port{owns,health,delay,log};if(mode!="off")port.diagnosticCheckpoint=copy;
 RiscTest::FileSources files({root.c_str()});auto*r=new Runtime(port);assert(files.bind(*r));assert(r->prepare(root.c_str()));bool ok=r->run();
 assert(ok==(mode!="retained"));assert(stages==2);assert(writes==(mode=="off"?0u:1u));
 if(mode=="retained"){assert(r->retained());held=r;}else delete r;
 assert(mode=="off"||std::string(record.text)=="last-attempt=fixture.operation");
 std::printf("PASS actual Runtime last-attempt %s; retained/normal return preserves pre-call RAM; no post-false writes\n",mode.c_str());
}
