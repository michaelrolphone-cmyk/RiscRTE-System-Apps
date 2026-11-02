/* Full refresh sequence recovered from Vector Arcade c6_0.3.2 (Seengreat
 * UC8179 7.5-inch BW demo). No experimental partial LUT or grayscale claims.
 * BUSY is active LOW. SPI mode 3. Wire byte 1 = black in this full-refresh mode.
 */
#include "panel.h"
#include "frame.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
static panel_pins pins;
static spi_device_handle_t spi;
static uint8_t chunk[2000] __attribute__((aligned(4)));
#define TRY(x) do { esp_err_t e=(x); if(e!=ESP_OK) return e; } while(0)
static esp_err_t send(const void *data,size_t len,int dc) {
 gpio_set_level(pins.dc,dc);
 while(len) { size_t n=len>sizeof(chunk)?sizeof(chunk):len; memcpy(chunk,data,n);
  spi_transaction_t t={.length=n*8,.tx_buffer=chunk}; TRY(spi_device_polling_transmit(spi,&t)); data=(const uint8_t*)data+n; len-=n;
 } return ESP_OK;
}
static esp_err_t cmd(uint8_t c){return send(&c,1,0);}
static esp_err_t reg(uint8_t c,const uint8_t *d,size_t n){TRY(cmd(c));return send(d,n,1);}
#define REG(c,...) do { const uint8_t d[]={__VA_ARGS__}; TRY(reg(c,d,sizeof d)); }while(0)
static esp_err_t idle(int timeout) {
 int64_t until=esp_timer_get_time()/1000+timeout;
 while(!gpio_get_level(pins.busy)) { if(esp_timer_get_time()/1000>=until)return ESP_ERR_TIMEOUT;vTaskDelay(pdMS_TO_TICKS(5)); }
 return ESP_OK;
}
static esp_err_t handshake(int timeout) {
 /* A disconnected/high BUSY must never masquerade as completed hardware work. */
 int64_t until=esp_timer_get_time()/1000+500;
 while(gpio_get_level(pins.busy)){if(esp_timer_get_time()/1000>=until)return ESP_ERR_INVALID_RESPONSE;vTaskDelay(1);}
 return idle(timeout);
}
static esp_err_t power(void) {
 gpio_set_level(pins.rst,0);vTaskDelay(pdMS_TO_TICKS(10));gpio_set_level(pins.rst,1);vTaskDelay(pdMS_TO_TICKS(10));
 REG(0x01,0x07,0x07,0x3f,0x3f);REG(0x06,0x17,0x17,0x28,0x17);TRY(cmd(0x04));TRY(handshake(2000));
 REG(0x00,0x1f);REG(0x61,0x03,0x20,0x01,0xe0);REG(0x15,0);REG(0x50,0x10,0x07);REG(0x60,0x22);return ESP_OK;
}
esp_err_t panel_init(panel_pins p) {
 int v[]={p.busy,p.rst,p.dc,p.cs,p.mosi,p.sck};
 for(int i=0;i<6;i++){if(v[i]<0||v[i]>21||(v[i]>=11&&v[i]<=17)||!GPIO_IS_VALID_GPIO(v[i])||(i&& !GPIO_IS_VALID_OUTPUT_GPIO(v[i])))return ESP_ERR_INVALID_ARG;for(int j=0;j<i;j++)if(v[i]==v[j])return ESP_ERR_INVALID_ARG;}
 pins=p;
 gpio_config_t out={.pin_bit_mask=(1ULL<<p.rst)|(1ULL<<p.dc),.mode=GPIO_MODE_OUTPUT};TRY(gpio_config(&out));
 gpio_config_t in={.pin_bit_mask=1ULL<<p.busy,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};TRY(gpio_config(&in));
 spi_bus_config_t b={.mosi_io_num=p.mosi,.miso_io_num=-1,.sclk_io_num=p.sck,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=sizeof(chunk)};
 TRY(spi_bus_initialize(SPI2_HOST,&b,SPI_DMA_CH_AUTO));
 spi_device_interface_config_t d={.mode=3,.clock_speed_hz=4000000,.spics_io_num=p.cs,.queue_size=1};
 return spi_bus_add_device(SPI2_HOST,&d,&spi);
}
void panel_stop(void){if(!spi)return;uint8_t v=0xf7;reg(0x50,&v,1);cmd(0x02);idle(3000);v=0xa5;reg(0x07,&v,1);}
esp_err_t panel_probe(void){esp_err_t e=power();panel_stop();return e;}
esp_err_t panel_render(const uint8_t *frame) {
 esp_err_t e=power();if(e!=ESP_OK){panel_stop();return e;}
 e=cmd(0x10);uint8_t blank[100]={0};
 for(int y=0;e==ESP_OK&&y<480;y++)e=send(blank,sizeof blank,1);
 if(e==ESP_OK)e=cmd(0x13);
 if(e==ESP_OK)e=send(frame,FRAME_BYTES,1);
 if(e==ESP_OK)e=cmd(0x12);
 if(e==ESP_OK)e=handshake(20000);
 panel_stop();return e;
}
