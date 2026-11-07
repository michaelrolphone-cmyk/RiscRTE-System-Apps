#include "panel.h"
#include "frame.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
static int64_t clock_ms;static int dc,command,busy_reads,busy_mode,fail_command;
static size_t old_size,new_size;static uint8_t old_data[48000],new_data[48000];static bool refresh;
esp_err_t gpio_config(const gpio_config_t *c){(void)c;return ESP_OK;}
int gpio_get_level(int p){(void)p;if(busy_mode==1)return 1;if(busy_mode==2)return 0;return busy_reads++>0;}
void gpio_set_level(int p,int v){if(p==3)dc=v;}
void vTaskDelay(int n){clock_ms+=n;}
int64_t esp_timer_get_time(void){return clock_ms*1000;}
esp_err_t spi_bus_initialize(int h,const spi_bus_config_t *b,int dma){(void)dma;assert(h==SPI2_HOST);assert(b->mosi_io_num==4&&b->sclk_io_num==5);return ESP_OK;}
esp_err_t spi_bus_add_device(int h,const spi_device_interface_config_t *c,spi_device_handle_t *out){(void)h;assert(c->mode==3&&c->spics_io_num==7);*out=(void*)1;return ESP_OK;}
esp_err_t spi_device_polling_transmit(spi_device_handle_t h,spi_transaction_t *t){(void)h;size_t n=t->length/8;const uint8_t *data=t->tx_buffer;
 if(!dc){assert(n==1);command=data[0];if(command==fail_command)return 99;if(command==4||command==0x12)busy_reads=0;if(command==0x12)refresh=true;return ESP_OK;}
 if(command==0x10){assert(old_size+n<=48000);memcpy(old_data+old_size,data,n);old_size+=n;}
 if(command==0x13){assert(new_size+n<=48000);memcpy(new_data+new_size,data,n);new_size+=n;}
 return ESP_OK;
}
static void reset(void){clock_ms=0;old_size=new_size=0;refresh=false;busy_mode=0;fail_command=-1;}
int main(void){panel_pins p={2,6,3,7,4,5};assert(panel_init(p)==ESP_OK);uint8_t frame[48000];for(int i=0;i<48000;i++)frame[i]=(i*37)&255;
 reset();assert(panel_probe()==ESP_OK);assert(!refresh&&new_size==0);
 reset();assert(panel_render(frame)==ESP_OK);assert(refresh&&old_size==48000&&new_size==48000);assert(memcmp(frame,new_data,48000)==0);for(int i=0;i<48000;i++)assert(old_data[i]==0);
 reset();busy_mode=1;assert(panel_render(frame)==ESP_ERR_INVALID_RESPONSE);assert(!refresh&&new_size==0);
 reset();busy_mode=2;assert(panel_render(frame)==ESP_ERR_TIMEOUT);assert(!refresh);
 reset();fail_command=0x13;assert(panel_render(frame)!=ESP_OK);assert(!refresh);
 reset();fail_command=0x02;assert(panel_render(frame)!=ESP_OK);assert(refresh);
 reset();fail_command=0x07;assert(panel_render(frame)!=ESP_OK);assert(refresh);
 reset();fail_command=0x02;assert(panel_probe()!=ESP_OK);assert(!refresh);
 reset();fail_command=0x07;assert(panel_probe()!=ESP_OK);assert(!refresh);
 puts("actual panel driver: pins, payload/polarity, BUSY probe, timeout and SPI failure tests passed");
}
