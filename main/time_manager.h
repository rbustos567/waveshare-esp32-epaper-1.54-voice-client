#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <time.h>
#include <sys/time.h>
#include "esp_err.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *label;      // Texto para la UI (ej. "UTC-6 (CST)")
    const char *tz_string;  // Cadena POSIX para el entorno ESP-IDF (ej. "CST6CDT,M3.2.0,M11.1.0")
    int offset_hours;       // Valor numérico para el editor (-6)
} tz_option_t;

#define TZ_INDEX_DEFAULT 2
#define NUM_TZ_OPTIONS (sizeof(TZ_OPTIONS) / sizeof(tz_option_t))

esp_err_t time_manager_init(void);
esp_err_t time_manager_sync_sntp(uint32_t timeout_ms);
esp_err_t time_manager_set_timezone(const char *tz_string);
esp_err_t time_manager_get_local_time(struct tm *timeinfo);
esp_err_t time_manager_get_formatted_time(char *buffer, size_t max_len, const char *format);
esp_err_t uptime_manager_init(void);
void uptime_prepare_for_sleep(void);
uint32_t uptime_get_seconds(void);
esp_err_t uptime_get_formatted(char *buffer, size_t max_len);
int get_current_tz_index(void);
void set_current_tz_index(int idx);
size_t timezone_get_options_count(void);
const tz_option_t *timezone_get_option_at(size_t index);
const char* get_tz_label_wrapper(int index);

#ifdef __cplusplus
}
#endif

#endif // TIME_MANAGER_H
