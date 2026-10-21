#include "battery_manager.h"

static const char *TAG = "battery_manager";

// Cache estático del porcentaje de batería
static int s_cached_battery_pct = -1;

uint32_t battery_get_voltage_mv(void)
{
    // 1. Si tu placa usa un GPIO para habilitar el divisor de voltaje, encenderlo
#ifdef BATTERY_PWR_EN_GPIO
    gpio_reset_pin(BATTERY_PWR_EN_GPIO);
    gpio_set_direction(BATTERY_PWR_EN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BATTERY_PWR_EN_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10)); // Tiempo de estabilización
#endif

    // 2. Resetear el pin del ADC para quitar cualquier aislamiento de Deep Sleep
    gpio_reset_pin(BATTERY_ADC_GPIO);

    // 3. Inicializar Unidad ADC
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // 4. Configurar Canal
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = BATTERY_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &config));

    // 5. Esquema de Calibración
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    bool calibrated = (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK);

    // 6. Muestreo de lecturas
    int raw_sum = 0;
    const int samples = 8;
    for (int i = 0; i < samples; i++) {
        int raw = 0;
        adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &raw);
        raw_sum += raw;
    }
    int raw_avg = raw_sum / samples;

    // IMPRESIÓN DE DIAGNÓSTICO:
    ESP_LOGI(TAG, "Lectura RAW Promedio del ADC: %d", raw_avg);

    // 7. Conversión a mV
    int voltage_adc_mv = 0;
    if (calibrated) {
        adc_cali_raw_to_voltage(cali_handle, raw_avg, &voltage_adc_mv);
    } else {
        voltage_adc_mv = (raw_avg * 3300) / 4095;
    }

    // 8. Liberación de recursos
    if (calibrated && cali_handle != NULL) {
        adc_cali_delete_scheme_curve_fitting(cali_handle);
    }
    if (adc_handle != NULL) {
        adc_oneshot_del_unit(adc_handle);
    }

    // Apagar el divisor de voltaje si se habilitó para ahorrar batería
#ifdef BATTERY_PWR_EN_GPIO
    gpio_set_level(BATTERY_PWR_EN_GPIO, 0);
#endif

    return (uint32_t)(voltage_adc_mv * VOLTAGE_DIVIDER_RATIO);
}

/**
 * @brief Mide el voltaje real por ADC, actualiza la variable caché y la retorna.
 * Debe llamarse solo al encender / despertar en app_main().
 */
int battery_update_cache(void)
{
    uint32_t voltage_mv = battery_get_voltage_mv();

    if (voltage_mv >= BATTERY_MAX_MV) {
        s_cached_battery_pct = 100;
    } else if (voltage_mv <= BATTERY_MIN_MV) {
        s_cached_battery_pct = 0;
    } else {
        // Mapeo lineal simple entre MIN y MAX
        s_cached_battery_pct = (int)(((float)(voltage_mv - BATTERY_MIN_MV) / 
                                      (BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0f);
    }

    ESP_LOGI(TAG, "Batería actualizada vía ADC: %d mV (%d%%)", (int)voltage_mv, s_cached_battery_pct);
    return s_cached_battery_pct;
}

/**
 * @brief Retorna el valor en caché sin activar el ADC ni el Power Gate.
 * Si nunca se ha medido, hace una medición inicial de seguridad.
 */
int battery_get_percentage(void)
{
    // Si por alguna razón no se ha inicializado el caché, realiza la medición una vez
    if (s_cached_battery_pct < 0) {
        return battery_update_cache();
    }

    ESP_LOGI(TAG, "Batería: (%d%%)", s_cached_battery_pct);

    return s_cached_battery_pct;
}
