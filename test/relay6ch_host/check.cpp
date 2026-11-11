#include "Board.h"
#include "stubs.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace RelayBoard;
int main(){
 assert(begin());JsonDocument in,out;
 auto run=[&](const char* src){in.clear();out.clear();assert(!deserializeJson(in,src));return execute(nullptr,"device.outputs.batch",in.as<JsonObjectConst>(),out.to<JsonObject>());};
 JsonDocument tone;tone["frequency_hz"]=440;tone["duration_ms"]=2;assert(execute(nullptr,"sound.buzzer",tone.as<JsonObjectConst>(),out.to<JsonObject>())==0&&physicalHz==0);
 assert(run(R"({"steps":[{"on":[1]},{"on":[7]}]})")==-1&&physicalMask==0);
 assert(run(R"({"steps":[{"on":[2,5],"rgb":[128,12,3],"tone_hz":825,"wait_ms":35},{"off":[2],"on":[6],"tone_hz":0},{"wait_ms":15}]})")==1);
 int status;while((status=poll(nullptr,out.as<JsonObject>()))==1)vTaskDelay(1);
 assert(status==0&&out["steps_completed"]==3&&out["relay_masks"][0]==18&&out["relay_masks"][1]==48);
 assert(physicalMask==48&&physicalRgb==0x800c03&&physicalHz==0&&out["elapsed_ms"].as<unsigned>()>=50);
 assert(run(R"({"steps":[{"on":[1],"tone_hz":880,"wait_ms":3000},{"on":[3]}]})")==1);
 for(int i=0;i<100&&physicalHz!=880;++i)vTaskDelay(1);assert(physicalHz==880);
 for(int i=0;i<100&&!cancel(nullptr);++i)vTaskDelay(1);
 assert(physicalMask==48&&physicalHz==0&&physicalRgb==0x800c03);
 assert(run(R"({"steps":[{"off":[5,6],"rgb":[0,0,0]}]})")==1);
 while((status=poll(nullptr,out.as<JsonObject>()))==1)vTaskDelay(1);
 assert(status==0&&physicalMask==0&&physicalRgb==0);

 // Actual production GPIO task: repeated same-mask intents hold continuously.
 assert(streamBegin());uint64_t sequence=1;
 auto mono=[](){return uint64_t(esp_timer_get_time()/1000);};
 streamDesired(1,mono()+500,sequence++);
 for(int i=0;i<100&&physicalMask.load()!=1;++i)vTaskDelay(1);
 assert(physicalMask.load()==1);
 const auto transitions=streamTransitions(),expirations=streamExpirations();
 for(int i=0;i<12;++i){
  streamDesired(1,mono()+500,sequence++);vTaskDelay(100);
  assert(physicalMask.load()==1&&streamTransitions()==transitions&&streamExpirations()==expirations);
 }
 streamDesired(0,mono()+500,sequence++);
 for(int i=0;i<100&&physicalMask.load();++i)vTaskDelay(1);
 assert(physicalMask.load()==0&&streamTransitions()==transitions+1&&streamExpirations()==expirations);
 streamDesired(2,mono()+30,sequence++);
 for(int i=0;i<100&&physicalMask.load()!=2;++i)vTaskDelay(1);
 assert(physicalMask.load()==2);vTaskDelay(50);
 assert(physicalMask.load()==0&&streamExpirations()==expirations+1);
 streamEnd();
 puts("Production board driver: invalid tail causes zero writes, multiple relay sets, timing, RGB wire bytes, tone stop, final-state retention, cancellation preserves untouched channels and color: PASS");fflush(stdout);std::_Exit(0);
}
