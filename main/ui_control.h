#ifndef UI_CONTROL_H
#define UI_CONTROL_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "ui_menu_manager.h"
#include "ui_renderer.h"
#include "board_button.h"
#include "esp_err.h"
#include "power_manager.h"
#include "audio_manager.h"
#include "callbacks.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define UI_BUTTON_QUEUE_LEN    5
#define UI_BUTTON_TASK_NAME          "ButtonTask"
#define UI_MENU_TASK_NAME            "MenuTask"
#define UI_BUTTON_TASK_STACK_SIZE    3584
#define UI_MENU_TASK_STACK_SIZE      4096
#define UI_BUTTON_TASK_PRIO          2
#define UI_MENU_TASK_PRIO          1

typedef enum {
    EVENT_NONE = 0,
    EVENT_SINGLE_CLICK,
    EVENT_DOUBLE_CLICK
} button_event_t;

typedef enum {
    UI_STATE_SPLASH,
    UI_STATE_MENU,
    UI_STATE_EDIT_VALUE,
    UI_STATE_VOICE_RECORDING,
    UI_STATE_MAX
} ui_state_t;

esp_err_t ui_control_init(void);
ui_state_t get_current_ui_state(void);
esp_err_t set_current_ui_state(ui_state_t);
bool get_cancel_voice_session(void);
esp_err_t set_cancel_voice_session(bool state);
bool get_is_waiting_response(void);
esp_err_t set_is_waiting_response(bool state);

#ifdef __cplusplus
}
#endif

#endif // UI_CONTROL_H
