#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <stdint.h>  
#include <stdbool.h> 
#include <stddef.h> 
#include "GUI_Paint.h"
#include "epd_display.h"
#include "ui_menu_manager.h"
#include "ui_control.h"
#include "esp_err.h"
#include <time.h>
#include "battery_manager.h"
#include "time_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ITEM_HEIGHT   28 
#define HEADER_HEIGHT 30
#define UI_MENU_TITLE_FONT &Font16
#define UI_MENU_ITEM_FONT  &Font16

esp_err_t render(const char *st_msg, const char *output);
esp_err_t render_full(const char *status_msg, const char *output_msg);
esp_err_t render_partial_with_text(const char *status_msg, const char *output_msg);
esp_err_t render_partial(void);
esp_err_t render_menu_to_buffer(Menu *current_menu);
esp_err_t render_splash_to_buffer(const char *status_msg);
esp_err_t render_message_to_buffer(const char *message, UWORD x, UWORD y);
esp_err_t render_init(void);
esp_err_t render_splash_screen(void);
esp_err_t render_draw_battery_icon_top_right(int percentage);
esp_err_t render_edit_screen_to_buffer(void);
esp_err_t render_voice_screen_to_buffer(const char *status_msg, const char *server_output_msg);

#ifdef __cplusplus
}
#endif

#endif // UI_RENDERER_H
