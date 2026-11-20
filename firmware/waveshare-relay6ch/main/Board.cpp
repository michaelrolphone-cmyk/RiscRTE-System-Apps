#include "Board.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/ledc.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace RelayBoard {
constexpr int pins[]={1,2,41,42,45,46};constexpr uart_port_t uart=UART_NUM_1;
static bool active=false;static rmt_channel_handle_t pixels=nullptr;static rmt_encoder_handle_t encoder=nullptr;
static std::atomic<uint8_t> mask{0};static uint8_t pixel[3]{};
static unsigned toneHz=0;
static std::atomic<bool> streamOwned{false};
struct DesiredOutput {uint8_t mask=0;uint64_t deadline=0,sequence=0;};
static std::mutex streamMutex;static DesiredOutput streamDesiredState;
static std::atomic<uint64_t> appliedSequence{0};
static std::atomic<uint32_t> streamChanges{0},streamExpired{0};
static TaskHandle_t streamTask=nullptr;
static bool setRelay(unsigned channel,bool on){
 const uint8_t bit=1u<<(channel-1);
 if(gpio_set_level(gpio_num_t(pins[channel-1]),on)!=ESP_OK)return false;
 mask=(mask&~bit)|(on?bit:0);return true;
}
static bool setTone(unsigned hz){
 if(hz){
  if(ledc_set_freq(LEDC_LOW_SPEED_MODE,LEDC_TIMER_0,hz)!=ESP_OK||
     ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,512)!=ESP_OK||
     ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0)!=ESP_OK)return false;
 }else if(ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0)!=ESP_OK)return false;
 toneHz=hz;return true;
}
static bool setPixel(const uint8_t rgb[3]){
 rmt_symbol_word_t words[25]{};
 for(unsigned i=0;i<24;++i){
  bool bit=rgb[i/8]&(0x80>>(i%8));
  words[i].level0=1;words[i].duration0=bit?8:4;words[i].level1=0;words[i].duration1=bit?4:8;
 }
 words[24].level0=0;words[24].duration0=500;words[24].level1=0;words[24].duration1=500;
 rmt_transmit_config_t tx{};
 if(rmt_transmit(pixels,encoder,words,sizeof(words),&tx)!=ESP_OK||rmt_tx_wait_all_done(pixels,20)!=ESP_OK)return false;
 memcpy(pixel,rgb,3);return true;
}

static bool batchStateBusy();
static void runStream(void*){
 for(;;){
  if(streamOwned.load()){
   const uint64_t now=esp_timer_get_time()/1000;
   DesiredOutput frame;{std::lock_guard<std::mutex> guard(streamMutex);frame=streamDesiredState;}
   const uint8_t desired=now<frame.deadline?frame.mask:0;
   const uint8_t old=mask;
   // Break before make and avoid toggling contacts for same-state intent.
   bool ok=true;
   for(unsigned ch=1;ch<=6;++ch)if((old&~desired)&(1u<<(ch-1)))ok=setRelay(ch,false)&&ok;
   if(ok)for(unsigned ch=1;ch<=6&&ok;++ch)if((desired&~old)&(1u<<(ch-1)))ok=setRelay(ch,true);
   if(ok){
    if(old!=desired){++streamChanges;if(old&&!desired&&now>=frame.deadline)++streamExpired;}
    appliedSequence.store(frame.sequence);
   }else{std::lock_guard<std::mutex> guard(streamMutex);streamDesiredState.mask=0;streamDesiredState.deadline=0;}
  }
  vTaskDelay(pdMS_TO_TICKS(5));
 }
}
bool streamBegin(){
 if(!active||streamOwned.load()||batchStateBusy())return false;
 {std::lock_guard<std::mutex> guard(streamMutex);streamDesiredState={};}
 appliedSequence=0;streamOwned=true;return true;
}
void streamDesired(uint8_t desired,uint64_t deadline,uint64_t sequence){
 std::lock_guard<std::mutex> guard(streamMutex);streamDesiredState={desired,deadline,sequence};
}
void streamNeutral(){std::lock_guard<std::mutex> guard(streamMutex);streamDesiredState.mask=0;streamDesiredState.deadline=0;}
void streamEnd(){
 streamNeutral();
 // Keep local task custody until neutral has been applied.
 for(unsigned i=0;i<20&&mask;++i)vTaskDelay(pdMS_TO_TICKS(5));
 streamOwned=false;
}
uint64_t streamApplied(){return appliedSequence.load();}
uint8_t streamOutput(){return mask;}
uint32_t streamTransitions(){return streamChanges.load();}
uint32_t streamExpirations(){return streamExpired.load();}

enum class BatchState { Idle, Running, Cancelling, Complete, Failed };
static std::atomic<BatchState> batchState{BatchState::Idle};
static bool batchStateBusy(){return batchState.load()!=BatchState::Idle;}
static TaskHandle_t batchTask=nullptr;
static Infra::OutputFrame frames[Infra::OutputFrameLimit];
static unsigned frameCount=0,completed=0,elapsedMs=0;
static uint8_t masks[Infra::OutputFrameLimit]{};
static unsigned appliedAt[Infra::OutputFrameLimit]{};

// The single physical job owns the GPIOs while this task runs. The agent task
// continues its network work. Release/acquire publishes immutable inputs and results.
static void runFrames(void*){
 for(;;){
  ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
  if(batchState.load()!=BatchState::Running&&batchState.load()!=BatchState::Cancelling)continue;
  const int64_t start=esp_timer_get_time();uint8_t touched=0;bool usedTone=false,usedPixel=false,ok=true;
  for(unsigned i=0;i<frameCount&&ok;++i){
   if(batchState.load()!=BatchState::Running){ok=false;break;}
   const auto& f=frames[i];
   // Break before make when a frame changes multiple relay channels.
   touched|=f.on|f.off;
   for(unsigned ch=1;ch<=6&&ok;++ch)if(f.off&(1u<<(ch-1)))ok=setRelay(ch,false);
   if(ok&&f.hasRgb){usedPixel=true;ok=setPixel(f.rgb);}
   for(unsigned ch=1;ch<=6&&ok;++ch)if(f.on&(1u<<(ch-1)))ok=setRelay(ch,true);
   if(ok&&f.tone>=0){usedTone=true;ok=setTone(f.tone);}
   if(!ok)break;
   masks[i]=mask;appliedAt[i]=(esp_timer_get_time()-start)/1000;++completed;
   const int64_t until=esp_timer_get_time()+int64_t(f.waitMs)*1000;
   for(;;){
    if(batchState.load()!=BatchState::Running){ok=false;break;}
    const int64_t left=until-esp_timer_get_time();if(left<=0)break;
    ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS((left+999)/1000)+1);
   }
  }
  elapsedMs=(esp_timer_get_time()-start)/1000;
  BatchState expected=BatchState::Running;
  if(ok&&batchState.compare_exchange_strong(expected,BatchState::Complete))continue;
  // Abort only the outputs addressed by this batch. Never replay partial work.
  for(unsigned ch=1;ch<=6;++ch)if(touched&(1u<<(ch-1)))setRelay(ch,false);
  if(usedTone)setTone(0);
  if(usedPixel){const uint8_t black[3]{};setPixel(black);}
  batchState.store(BatchState::Failed);
 }
}
bool cancel(void*){
 auto state=batchState.load();
 if(state==BatchState::Running){
  if(batchState.compare_exchange_strong(state,BatchState::Cancelling))xTaskNotifyGive(batchTask);
  return false;
 }
 if(state==BatchState::Cancelling)return false;
 batchState.store(BatchState::Idle);return true;
}
int poll(void*,JsonObject out){
 const auto state=batchState.load();
 if(state==BatchState::Running||state==BatchState::Cancelling)return 1;
 if(state!=BatchState::Complete&&state!=BatchState::Failed)return -1;
 out["steps_completed"]=completed;out["elapsed_ms"]=elapsedMs;out["commanded_mask"]=mask.load();
 out["tone_hz"]=toneHz;out["contact_feedback"]=false;
 auto rgb=out["rgb"].to<JsonArray>();for(auto c:pixel)rgb.add(c);
 auto stamps=out["applied_at_ms"].to<JsonArray>();auto states=out["relay_masks"].to<JsonArray>();
 for(unsigned i=0;i<completed;++i){stamps.add(appliedAt[i]);states.add(masks[i]);}
 batchState.store(BatchState::Idle);return state==BatchState::Complete?0:-1;
}

bool ready(void*){return active;}
bool begin(){
 // Dedicated static product: these exact resources are exclusively owned for
 // the whole boot. No arbitrary pin/controller or runtime ELF imports exist.
 for(auto pin:pins){if(gpio_set_level(gpio_num_t(pin),0)!=ESP_OK)return false;}
 gpio_config_t cfg{};for(auto pin:pins)cfg.pin_bit_mask|=1ULL<<pin;cfg.pin_bit_mask|=1ULL<<21;cfg.mode=GPIO_MODE_OUTPUT;
 if(gpio_set_level(GPIO_NUM_21,0)!=ESP_OK||gpio_config(&cfg)!=ESP_OK)return false;
 uart_config_t uc{};uc.baud_rate=9600;uc.data_bits=UART_DATA_8_BITS;uc.parity=UART_PARITY_DISABLE;uc.stop_bits=UART_STOP_BITS_1;uc.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;uc.source_clk=UART_SCLK_DEFAULT;
 if(uart_is_driver_installed(uart)||uart_param_config(uart,&uc)!=ESP_OK||uart_set_pin(uart,17,18,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE)!=ESP_OK||uart_driver_install(uart,512,0,0,nullptr,0)!=ESP_OK)return false;
 ledc_timer_config_t timer{};timer.speed_mode=LEDC_LOW_SPEED_MODE;timer.duty_resolution=LEDC_TIMER_10_BIT;timer.timer_num=LEDC_TIMER_0;timer.freq_hz=2000;timer.clk_cfg=LEDC_AUTO_CLK;
 ledc_channel_config_t tone{};tone.gpio_num=21;tone.speed_mode=LEDC_LOW_SPEED_MODE;tone.channel=LEDC_CHANNEL_0;tone.timer_sel=LEDC_TIMER_0;tone.duty=0;
 if(ledc_timer_config(&timer)!=ESP_OK||ledc_channel_config(&tone)!=ESP_OK)return false;
 rmt_tx_channel_config_t rc{};rc.gpio_num=GPIO_NUM_38;rc.clk_src=RMT_CLK_SRC_DEFAULT;rc.resolution_hz=10000000;rc.mem_block_symbols=64;rc.trans_queue_depth=1;
 rmt_copy_encoder_config_t ec{};
 if(rmt_new_tx_channel(&rc,&pixels)!=ESP_OK||rmt_new_copy_encoder(&ec,&encoder)!=ESP_OK||rmt_enable(pixels)!=ESP_OK)return false;
 if(xTaskCreate(runStream,"output-stream",3072,nullptr,5,&streamTask)!=pdPASS)return false;
 if(xTaskCreate(runFrames,"output-batch",4096,nullptr,4,&batchTask)!=pdPASS)return false;
 active=true;JsonDocument in,out;in["red"]=0;in["green"]=0;in["blue"]=0;return execute(nullptr,"light.rgb",in.as<JsonObjectConst>(),out.to<JsonObject>())==0;
}
int execute(void*,const char* c,JsonObjectConst in,JsonObject out){
 using namespace Infra;
 const bool batch=!strcmp(c,"device.outputs.batch");
 const char* op=batch?"apply":!strcmp(c,"serial.rs485")?"exchange":!strcmp(c,"sound.buzzer")?"tone":"set";
 if(!active||streamOwned.load()||batchState.load()!=BatchState::Idle||!hardwareInput(c,op,in))return -1;
 if(batch){
  unsigned totalMs=0;if(!decodeOutputBatch(in,frames,totalMs))return -1;
  frameCount=in["steps"].size();completed=elapsedMs=0;
  batchState.store(BatchState::Running);xTaskNotifyGive(batchTask);return 1;
 }
 if(!strcmp(c,"relay.bank")){
  unsigned channel=in["channel"];bool on=in["on"];if(!setRelay(channel,on))return -1;
  out["channel"]=channel;out["on"]=on;out["commanded_mask"]=mask.load();out["contact_feedback"]=false;return 0;
 }
 if(!strcmp(c,"sound.buzzer")){
  if(!setTone(in["frequency_hz"]))return -1;
  vTaskDelay(pdMS_TO_TICKS(in["duration_ms"].as<unsigned>()));
  if(!setTone(0))return -1;
  out["duration_ms"]=in["duration_ms"];out["frequency_hz"]=in["frequency_hz"];return 0;
 }
 if(!strcmp(c,"light.rgb")){
  // This board transmits RGB in wire order.
  uint8_t rgb[]={in["red"],in["green"],in["blue"]};if(!setPixel(rgb))return -1;
  out["red"]=rgb[0];out["green"]=rgb[1];out["blue"]=rgb[2];return 0;
 }
 const auto parity=in["parity"]=="even"?UART_PARITY_EVEN:in["parity"]=="odd"?UART_PARITY_ODD:UART_PARITY_DISABLE;
 if(uart_set_baudrate(uart,in["baud"])!=ESP_OK||uart_set_parity(uart,parity)!=ESP_OK||uart_set_stop_bits(uart,in["stop_bits"]==2?UART_STOP_BITS_2:UART_STOP_BITS_1)!=ESP_OK||uart_flush_input(uart)!=ESP_OK)return -1;
 // Direction is handled by the board's automatic transceiver circuit.
 uint8_t tx[256],rx[256];const char* hex=in["tx_hex"];unsigned count=strlen(hex)/2;auto nibble=[](char x){return x<='9'?x-'0':x-'a'+10;};for(unsigned i=0;i<count;++i)tx[i]=(nibble(hex[2*i])<<4)|nibble(hex[2*i+1]);
 int64_t deadline=esp_timer_get_time()+1000LL*in["timeout_ms"].as<unsigned>();int sent=uart_write_bytes(uart,tx,count);out["tx_accepted_bytes"]=sent>0?sent:0;
 if(sent!=int(count))return -1;
 const auto remaining=[&](){auto us=deadline-esp_timer_get_time();return us>0?pdMS_TO_TICKS((us+999)/1000):TickType_t(0);};
 if(uart_wait_tx_done(uart,remaining())!=ESP_OK)return -1;
 unsigned got=0,maximum=in["rx_max"];while(got<maximum&&esp_timer_get_time()<deadline){int n=uart_read_bytes(uart,rx+got,maximum-got,pdMS_TO_TICKS(10));if(n<0)return -1;got+=n;}
 char value[513];for(unsigned i=0;i<got;++i)snprintf(value+2*i,3,"%02x",rx[i]);value[got*2]=0;out["rx_hex"]=value;out["timed_out"]=got<maximum;out["tx_completed"]=true;return 0;
}
}
