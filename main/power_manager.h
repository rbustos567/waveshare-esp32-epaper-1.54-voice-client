#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "time_manager.h"
#include "ui_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *label;      // Text to show on UI (example: "30 s", "1 min")
    int value_seconds;      // Real value in seconds (30, 60, 120, 300)
} sleep_timeout_option_t;

// Standard BOOT button on most ESP32 boards is mapped to GPIO 0
#define BOOT_BUTTON_GPIO     GPIO_NUM_0

// BOOT button connects pin to GND when pressed (Active Low = 0)
#define BOOT_BUTTON_LEVEL    0

#define SLEEP_TIMEOUT_INDEX_DEFAULT 2

esp_err_t enter_deep_sleep_boot_button(void);
esp_err_t power_manager_init(void);
esp_err_t power_manager_reset_inactivity(void);
size_t power_manager_get_timeout_options_count(void);
const sleep_timeout_option_t* power_manager_get_timeout_option_at(size_t index);
int power_manager_get_current_timeout_index(void);
esp_err_t power_manager_set_timeout_index(int index);
const char* get_timeout_label_wrapper(int index);

#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGER_H
