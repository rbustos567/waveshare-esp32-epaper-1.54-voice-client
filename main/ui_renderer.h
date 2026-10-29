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
#define UI_MENU_TITLE_FONT          &Font16
#define UI_MENU_ITEM_FONT           &Font16
#define UI_MENU_BATTERY_FONT        &Font12
#define UI_MENU_DATE_FONT           &Font12
#define UI_MENU_TIME_FONT           &Font24
#define UI_MENU_TIMEZONE_FONT       &Font12
#define UI_MENU_SYS_UPTIME_T_FONT   &Font12
#define UI_MENU_SYS_UPTIME_FONT     &Font16
#define UI_MENU_STATUS_FONT         &Font12
#define UI_REC_STATUS_FONT          &Font12
#define UI_REC_SVR_OUT_FONT         &Font12
#define UI_REC_CLICK_MSG_FONT       &Font12

#define UI_MENU_SYS_UPTIME_STR "SYSTEM UPTIME"
#define UI_MENU_STATUS_T_STR   "STATUS:"
#define UI_REC_STATUS_T_STR    "STATUS:"
#define UI_REC_2_CLK_STR       "2-Click: Rec/Stop"
#define UI_REC_1_CLK_STR       "1-Click: Exit"

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
