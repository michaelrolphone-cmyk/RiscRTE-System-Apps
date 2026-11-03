#include "Board.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/ledc.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace RelayBoard {
constexpr int pins[]={1,2,41,42,45,46};constexpr uart_port_t uart=UART_NUM_1;
static bool active=false;static rmt_channel_handle_t pixels=nullptr;static rmt_encoder_handle_t encoder=nullptr;
static uint8_t mask=0;
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
 active=true;JsonDocument in,out;in["red"]=0;in["green"]=0;in["blue"]=0;return execute(nullptr,"light.rgb",in.as<JsonObjectConst>(),out.to<JsonObject>())==0;
}
int execute(void*,const char* c,JsonObjectConst in,JsonObject out){
 using namespace Infra;if(!active||!hardwareInput(c,!strcmp(c,"serial.rs485")?"exchange":!strcmp(c,"sound.buzzer")?"tone":"set",in))return -1;
 if(!strcmp(c,"relay.bank")){unsigned ch=in["channel"].as<unsigned>()-1;bool on=in["on"];if(gpio_set_level(gpio_num_t(pins[ch]),on)!=ESP_OK)return -1;mask=(mask&~(1u<<ch))|(unsigned(on)<<ch);out["channel"]=ch+1;out["on"]=on;out["commanded_mask"]=mask;out["contact_feedback"]=false;return 0;}
 if(!strcmp(c,"sound.buzzer")){
  if(!ledc_set_freq(LEDC_LOW_SPEED_MODE,LEDC_TIMER_0,in["frequency_hz"])||ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,512)!=ESP_OK||ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0)!=ESP_OK)return -1;
  vTaskDelay(pdMS_TO_TICKS(in["duration_ms"].as<unsigned>()));if(ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0)!=ESP_OK)return -1;out["duration_ms"]=in["duration_ms"];out["frequency_hz"]=in["frequency_hz"];return 0;
 }
 if(!strcmp(c,"light.rgb")){
  uint8_t bytes[]={in["green"],in["red"],in["blue"]};rmt_symbol_word_t words[25]{};
  for(unsigned i=0;i<24;++i){bool bit=bytes[i/8]&(0x80>>(i%8));words[i].level0=1;words[i].duration0=bit?8:4;words[i].level1=0;words[i].duration1=bit?4:8;}
  words[24].level0=0;words[24].duration0=500;words[24].level1=0;words[24].duration1=500;
  rmt_transmit_config_t tx{};if(rmt_transmit(pixels,encoder,words,sizeof(words),&tx)!=ESP_OK||rmt_tx_wait_all_done(pixels,20)!=ESP_OK){active=false;return -1;}
  out["red"]=in["red"];out["green"]=in["green"];out["blue"]=in["blue"];return 0;
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
