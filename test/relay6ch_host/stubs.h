#pragma once
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <algorithm>
using namespace std::chrono;
constexpr int ESP_OK=0, GPIO_MODE_OUTPUT=1, GPIO_NUM_21=21,GPIO_NUM_38=38, UART_NUM_1=1, UART_DATA_8_BITS=8, UART_PARITY_DISABLE=0,UART_PARITY_EVEN=1,UART_PARITY_ODD=2,UART_STOP_BITS_1=1,UART_STOP_BITS_2=2,UART_HW_FLOWCTRL_DISABLE=0,UART_SCLK_DEFAULT=0,UART_PIN_NO_CHANGE=-1, LEDC_LOW_SPEED_MODE=0, LEDC_TIMER_10_BIT=10,LEDC_TIMER_0=0,LEDC_AUTO_CLK=0,LEDC_CHANNEL_0=0,RMT_CLK_SRC_DEFAULT=0,pdTRUE=1,pdPASS=1;
using gpio_num_t=int;using uart_port_t=int;using TickType_t=unsigned;using rmt_channel_handle_t=void*;using rmt_encoder_handle_t=void*;using TaskHandle_t=void*;
struct gpio_config_t{uint64_t pin_bit_mask;int mode;};
struct uart_config_t{int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk;};
struct ledc_timer_config_t{int speed_mode,duty_resolution,timer_num,freq_hz,clk_cfg;};
struct ledc_channel_config_t{int gpio_num,speed_mode,channel,timer_sel,duty;};
struct rmt_tx_channel_config_t{int gpio_num,clk_src,resolution_hz,mem_block_symbols,trans_queue_depth;};
struct rmt_copy_encoder_config_t{};struct rmt_transmit_config_t{};struct rmt_symbol_word_t{unsigned level0,duration0,level1,duration1;};
inline std::atomic<unsigned> physicalMask{0},physicalHz{0},physicalRgb{0};
inline int gpio_set_level(int pin,int on){int pins[]={1,2,41,42,45,46};for(unsigned i=0;i<6;++i)if(pin==pins[i]){if(on)physicalMask.fetch_or(1u<<i);else physicalMask.fetch_and(~(1u<<i));}return 0;}
// ESP-IDF returns an esp_err_t; frequency is not the return value.
inline unsigned timerHz=0,duty=0;
inline int ledc_set_freq(int,int,unsigned hz){timerHz=hz;return ESP_OK;}
inline int ledc_set_duty(int,int,unsigned value){duty=value;return ESP_OK;}
inline int ledc_update_duty(int,int){physicalHz=duty?timerHz:0;return ESP_OK;}
inline int ledc_stop(int,int,int){physicalHz=0;return 0;}
inline int rmt_transmit(void*,void*,const rmt_symbol_word_t* w,size_t,void*){unsigned rgb=0;for(unsigned i=0;i<24;++i)rgb=(rgb<<1)|(w[i].duration0==8);physicalRgb=rgb;return 0;}
inline int64_t esp_timer_get_time(){return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();}
constexpr TickType_t portMAX_DELAY=~TickType_t(0);
inline TickType_t pdMS_TO_TICKS(unsigned ms){return ms;}
inline std::mutex mx; inline std::condition_variable cv;inline unsigned events=0;
inline int xTaskCreate(void(*fn)(void*),const char*,unsigned,void*,unsigned,void** task){*task=(void*)1;std::thread([fn]{fn(nullptr);}).detach();return pdPASS;}
inline void xTaskNotifyGive(void*){{std::lock_guard<std::mutex> l(mx);++events;}cv.notify_all();}
inline int ulTaskNotifyTake(int,unsigned ms){std::unique_lock<std::mutex> l(mx);if(ms==portMAX_DELAY)cv.wait(l,[]{return events>0;});else cv.wait_for(l,milliseconds(ms),[]{return events>0;});auto e=events;events=0;return e;}
inline void vTaskDelay(unsigned ms){std::this_thread::sleep_for(milliseconds(ms));}
template<class... Args> inline int ok(Args...){return 0;}
#define gpio_config ok
#define uart_is_driver_installed(...) false
#define uart_param_config ok
#define uart_set_pin ok
#define uart_driver_install ok
#define ledc_timer_config ok
#define ledc_channel_config ok
#define rmt_new_tx_channel ok
#define rmt_new_copy_encoder ok
#define rmt_enable ok
#define rmt_tx_wait_all_done ok
#define uart_set_baudrate ok
#define uart_set_parity ok
#define uart_set_stop_bits ok
#define uart_flush_input ok
#define uart_write_bytes ok
#define uart_wait_tx_done ok
#define uart_read_bytes ok
