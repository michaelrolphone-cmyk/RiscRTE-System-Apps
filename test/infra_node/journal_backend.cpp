#include "Journal.h"
#include "AppDataFiles.h"
#ifdef NDEBUG
#error Storage integration tests require active assertions
#endif
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

// Use the production filesystem backend, including its SDK capacity check.
// Only the async worker handoff is synchronous in this host test.
static RiscStorage::AppDataFiles files({nullptr, [](void*) -> uint32_t { return 1; },
 [](void*) { return true; }, std::malloc, std::free});
static int32_t outcome=0;
static unsigned writes=0;
static int stat(void*,const char* name,uint32_t* size,uint64_t* rev){return files.stat(1,name,size,rev);}
static int read(void*,const char* name,uint64_t rev,void* out,uint32_t cap,uint32_t* size,uint64_t* fresh){return files.read(1,name,rev,out,cap,size,fresh);}
static int begin(void*,const char* name,uint64_t rev,const void* data,uint32_t size,uint64_t* token){++writes;outcome=files.replace(1,name,rev,data,size);*token=1;return 0;}
static int collect(void*,uint64_t token,int32_t* result){assert(token==1);*result=outcome;return 0;}
static risc_bound_app_data_async_v1 api{{1,sizeof(api),nullptr,stat,read,nullptr},RISC_BOUND_APP_DATA_ASYNC_TAG,1,begin,collect};

int main(){
 char root[]="/tmp/infra-storage-XXXXXX";assert(mkdtemp(root));assert(files.configure(root));
 const char* seed=R"({"schema_version":1,"installation_id":"legacy-installation","credential_generation":1,"credentials":{},"participant_id":"","cursor":0,"pending":[],"job":null,"checkpoints":{},"configuration_revision":1})";
 std::vector<uint8_t> frame(RISC_APP_DATA_FILE_MAX);
 auto size=Infra::frame(frame.data(),frame.size(),seed,strlen(seed),1);
 assert(size&&files.replace(1,"node-state.bin",0,frame.data(),size)==0);
 Infra::Journal journal;assert(journal.initialize(&api.base));
 assert(journal.load()==0&&journal.seq==1&&!journal.state["credentials"].size());
 // Existing frame format and empty credentials must remain readable for setup.
 assert(journal.state["installation_id"]=="legacy-installation");
 auto padding=journal.state["relay"]["test_padding"].to<JsonArray>();
 for(unsigned i=0;i<15;++i)padding.add(std::string(4096,char('a'+i)));
 assert(measureJson(journal.state)>60000&&journal.save()==1);
 assert(journal.collect()==0&&outcome==0&&journal.load()==0&&journal.seq==2);
 // Reject a too-large snapshot before writing; the last committed state survives.
 journal.state["relay"]["test_padding"].as<JsonArray>().add(std::string(4096,'z'));
 const auto before=writes;assert(journal.save()==-1&&writes==before);
 Infra::Journal reboot;assert(reboot.initialize(&api.base));
 assert(reboot.load()==0&&reboot.seq==2&&reboot.state["relay"]["test_padding"].size()==15);
 journal.dispose();reboot.dispose();assert(files.exitSafe());std::filesystem::remove_all(root);
 puts("Journal production backend: setup seed, >60 KB save/reboot, oversize refusal preserves committed state PASS");
}
