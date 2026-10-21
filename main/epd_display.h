#ifndef EPD_DISPLAY_H
#define EPD_DISPLAY_H

#include <stdint.h>  
#include <stdbool.h> 
#include <stddef.h> 
#include "DEV_Config.h"
#include "EPD_1in54_V2.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EPD_WIDTH  200
#define EPD_HEIGHT 200 
#define BUFFER_SIZE ((EPD_WIDTH * EPD_HEIGHT) / 8) // Framebuffer 1-bit per píxel (5000 bytes)

esp_err_t epd_display_init(void);
esp_err_t epd_display_update_full(void);
esp_err_t epd_display_update_partial(void);
uint8_t* epd_display_get_buffer(void);

#ifdef __cplusplus
}
#endif

#endif // EPD_DISPLAY_H
