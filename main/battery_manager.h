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

// Configuración de hardware según la placa
#define BATTERY_ADC_UNIT       ADC_UNIT_1
#define BATTERY_ADC_CHANNEL    ADC_CHANNEL_0    // Ej. GPIO1 en ESP32-S3 (Ajustar según PCB)
#define BATTERY_ADC_GPIO       GPIO_NUM_1       // Pin físico del ADC

// Si tu placa tiene un pin de habilitación de energía/batería (ejemplo: GPIO14)
// Si tu placa no usa pin de habilitación, puedes omitir BATTERY_PWR_EN
#define BATTERY_PWR_EN_GPIO    GPIO_NUM_14      // Ajustar al pin de tu PCB o comentar si no aplica

#define BATTERY_ADC_ATTEN      ADC_ATTEN_DB_12  // Rango hasta ~3.3V

// Parámetros de la batería LiPo (3.0V descargada - 4.2V cargada)
#define BATTERY_MIN_MV         3300             // 3.3V como 0% de seguridad
#define BATTERY_MAX_MV         4200             // 4.2V como 100%
#define VOLTAGE_DIVIDER_RATIO  2.0f             // Divisor de voltaje habitual (R1 = R2)


int battery_get_percentage(void);
uint32_t battery_get_voltage_mv(void);
int battery_update_cache(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MANAGER_H
