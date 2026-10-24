#include "epd_display.h"

// Monochromatic Frame buffer in RAM (0 = Black, 1 = White)
static uint8_t epd_buffer[BUFFER_SIZE];

// E-paper Screen Buffer
static UBYTE *screen_buffer = NULL;

esp_err_t epd_display_update_full(void) {
    esp_err_t ret = ESP_OK;

    EPD_1IN54_V2_Init();
    EPD_1IN54_V2_Display(screen_buffer);
    // These two lines fix the screen ghosting
    EPD_1IN54_V2_Init_Partial();
    EPD_1IN54_V2_DisplayPart(screen_buffer);

    EPD_1IN54_V2_Sleep();

    return ret;
}

esp_err_t epd_display_update_partial(void) {
    esp_err_t ret = ESP_OK; 

    EPD_1IN54_V2_Init_Partial();
    EPD_1IN54_V2_DisplayPart(screen_buffer);

    EPD_1IN54_V2_Sleep();

    return ret;
}

esp_err_t epd_display_init(void) {
    esp_err_t ret = ESP_OK;

    DEV_Module_Init();
    EPD_1IN54_V2_Init();
    EPD_1IN54_V2_Clear();

    UWORD image_size = ((EPD_1IN54_V2_WIDTH % 8 == 0) ?
                       (EPD_1IN54_V2_WIDTH / 8) :
                       (EPD_1IN54_V2_WIDTH / 8 + 1)) * EPD_1IN54_V2_HEIGHT;


    screen_buffer = (UBYTE *)malloc(image_size);
    if (screen_buffer == NULL) {
        ret = ESP_ERR_NO_MEM;
    }

    return ret;
}

uint8_t* epd_display_get_buffer(void) {

    return screen_buffer;
}
