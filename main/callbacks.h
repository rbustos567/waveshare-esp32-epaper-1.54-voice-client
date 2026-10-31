#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "DEV_Config.h"
#include "ui_control.h"
#include "ui_renderer.h"
#include "ui_menu_manager.h"
#include "time_manager.h"
#include "power_manager.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "audio_manager.h"
#include "esp_http_client.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

void action_record_audio(void);
void return_to_splash_screen(void);
void action_open_tz_editor(void);
void action_save_timezone(int selected_index);
void action_open_sleep_timeout_editor(void);
void action_save_sleep_timeout(int selected_index);
void action_record_and_send_audio(void);
void action_open_audio_command_screen(void);
void action_cancel_voice_session(void);
void action_open_choose_network_editor(void);
void action_open_choose_server_editor(void);
void action_save_wifi_selection(int index);
void action_save_server_selection(int index);

#ifdef __cplusplus
}
#endif

#endif // CALLBACKS_H
