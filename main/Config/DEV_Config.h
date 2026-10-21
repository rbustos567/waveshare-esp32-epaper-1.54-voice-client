#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Debug.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Definición de tipos de datos de Waveshare
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

// Definición de Pines (Ajusta según el conexionado de tu ESP32/ESP32-S3)
//#define EPD_MOSI_PIN  11
//#define EPD_SCK_PIN   12
//#define EPD_CS_PIN    10
//#define EPD_DC_PIN    9
//#define EPD_RST_PIN   14
//#define EPD_BUSY_PIN  13
//#define EPD_PWR_PIN   6   // Pin de habilitación de energía

#define EPD_PWR_PIN   6
#define EPD_BUSY_PIN  8
#define EPD_RST_PIN   9
#define EPD_DC_PIN    10
#define EPD_CS_PIN    11
#define EPD_SCK_PIN   12
#define EPD_MOSI_PIN  13

// Macros para control de GPIO
#define DEV_Digital_Write(_pin, _value) gpio_set_level(_pin, _value)
#define DEV_Digital_Read(_pin)          gpio_get_level(_pin)
#define DEV_Delay_ms(__xms)             vTaskDelay((__xms) / portTICK_PERIOD_MS)

// Prototipos de funciones HAL
UBYTE DEV_Module_Init(void);
void  DEV_Module_Exit(void);
void  DEV_SPI_WriteByte(UBYTE value);
void  DEV_SPI_Write_nByte(UBYTE *pData, uint32_t Len);

#endif
