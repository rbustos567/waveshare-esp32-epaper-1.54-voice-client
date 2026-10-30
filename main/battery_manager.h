#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// Hardware configuration according to the board
#define BATTERY_ADC_CHANNEL   ADC_CHANNEL_3  // GPIO4
#define BATTERY_ADC_ATTEN     ADC_ATTEN_DB_12 // Range up to ~3.3V

// LiPo battery parameters (3.0V discharged - 4.2V charged)
#define BATTERY_MIN_MV          3300UL            // 3.3V as 0% safety threshold
#define BATTERY_MAX_MV          4200UL            // 4.2V as 100%
#define VOLTAGE_DIVIDER_RATIO   2.0f            // Standard voltage divider ratio (R1 = R2)
#define BATTERY_SAMPLES         32

esp_err_t battery_manager_init(void);
esp_err_t battery_get_percentage(int *out_percentage);
esp_err_t battery_get_voltage_mv(uint32_t *out_voltage_mv);
esp_err_t battery_update_cache(int *out_percentage);
int battery_get_last_raw_avg(void);
uint32_t battery_get_last_voltage_mv(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MANAGER_H
