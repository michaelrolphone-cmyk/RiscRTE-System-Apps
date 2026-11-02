#pragma once
#include <stddef.h>
#include "esp_err.h"
#define SPI2_HOST 2
#define SPI_DMA_CH_AUTO 0
typedef void *spi_device_handle_t;
typedef struct {size_t length;const void *tx_buffer;} spi_transaction_t;
typedef struct {int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,max_transfer_sz;} spi_bus_config_t;
typedef struct {int mode,clock_speed_hz,spics_io_num,queue_size;} spi_device_interface_config_t;
esp_err_t spi_bus_initialize(int,const spi_bus_config_t*,int);
esp_err_t spi_bus_add_device(int,const spi_device_interface_config_t*,spi_device_handle_t*);
esp_err_t spi_device_polling_transmit(spi_device_handle_t,spi_transaction_t*);
