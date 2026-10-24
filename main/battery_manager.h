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
#define BATTERY_ADC_UNIT       ADC_UNIT_1
#define BATTERY_ADC_CHANNEL    ADC_CHANNEL_0    // e.g. GPIO1 on ESP32-S3 (Adjust according to PCB)
#define BATTERY_ADC_GPIO       GPIO_NUM_1       // Physical ADC pin

// If your board has a power/battery enable pin (example: GPIO14)
// If your board does not use an enable pin, you can omit BATTERY_PWR_EN
#define BATTERY_PWR_EN_GPIO     GPIO_NUM_14     // Adjust to your PCB pin or comment out if not applicable

#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12 // Range up to ~3.3V

// LiPo battery parameters (3.0V discharged - 4.2V charged)
#define BATTERY_MIN_MV          3300            // 3.3V as 0% safety threshold
#define BATTERY_MAX_MV          4200            // 4.2V as 100%
#define VOLTAGE_DIVIDER_RATIO   2.0f            // Standard voltage divider ratio (R1 = R2)
						//
esp_err_t battery_get_percentage(int *out_percentage);
esp_err_t battery_get_voltage_mv(uint32_t *out_voltage_mv);
esp_err_t battery_update_cache(int *out_percentage);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MANAGER_H
