#include "battery_manager.h"

static const char *TAG = "BATTERY_MANAGER";

static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_cali_enabled = false;

// Static cache for battery percentage (-1 indicates uninitialized)
static int s_cached_battery_pct = -1;
// Static vairables for debugging purpose
static int s_last_raw_avg = 0;
static uint32_t s_last_voltage_mv = 0;

/**
 * @brief Inicializa el ADC y la calibración una sola vez (Patrón oficial Waveshare).
 */
esp_err_t battery_manager_init(void) {
    esp_err_t ret = ESP_OK;

    // 1. Configure curve fitting calibration scheme
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t cali_err = adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle);
    if (cali_err == ESP_OK) {
        s_cali_enabled = true;
        ESP_LOGI(TAG, "Curve fitting calibration initialized successfully.");
    } else {
        ESP_LOGI(TAG, "Curve fitting calibration not supported/failed (%s), falling back.", esp_err_to_name(cali_err));
        s_cali_enabled = false;
    }

    // 2. Initialize unit ADC1
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&init_config, &s_adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_oneshot_config_channel(s_adc1_handle, BATTERY_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    return ret;
}

/**
 * @brief Obtiene el voltaje en milivoltios usando el método oficial de Waveshare.
 */
esp_err_t battery_get_voltage_mv(uint32_t *out_voltage_mv) {
    esp_err_t ret = ESP_OK;

    if (out_voltage_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_adc1_handle == NULL) {
        ESP_LOGE(TAG, "ADC not initialized. Call battery_manager_init() first.");
        return ESP_ERR_INVALID_STATE;
    }

    // Take multiple samples to obtain average and reduce noise
    int raw_sum = 0;
    int raw = 0;

    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        ret = adc_oneshot_read(s_adc1_handle, BATTERY_ADC_CHANNEL, &raw);
        if (ret != ESP_OK) {
            return ret;
        }
        raw_sum += raw;
    }

    s_last_raw_avg = raw_sum / samples;
    int voltage_adc_mv = 0;

    // Official conversion using Espressif calibration
    if (s_cali_enabled) {
        adc_cali_raw_to_voltage(s_cali_handle, s_last_raw_avg, &voltage_adc_mv);
    } else {
        voltage_adc_mv = (s_last_raw_avg * 3300) / 4095;
    }

    // Applying voltage divider facto (x2 in waveshare boards)
    s_last_voltage_mv = (uint32_t)(voltage_adc_mv * VOLTAGE_DIVIDER_RATIO);
    *out_voltage_mv = s_last_voltage_mv;

    ESP_LOGD(TAG, "ADC Raw Avg: %d, Voltage mV: %lu", s_last_raw_avg, (unsigned long)s_last_voltage_mv);
    
    return ret;
}

int battery_get_last_raw_avg(void) {
    return s_last_raw_avg;
}

uint32_t battery_get_last_voltage_mv(void) {
    return s_last_voltage_mv;
}

esp_err_t battery_update_cache(int *out_percentage) {
    esp_err_t ret = ESP_OK;

    uint32_t voltage_mv = 0;
    ret = battery_get_voltage_mv(&voltage_mv);
    if (ret != ESP_OK) 
        return ret;

    if (voltage_mv >= BATTERY_MAX_MV) {
        s_cached_battery_pct = 100;
    } else if (voltage_mv <= BATTERY_MIN_MV) {
        s_cached_battery_pct = 0;
    } else {
        s_cached_battery_pct = (int)(((float)(voltage_mv - BATTERY_MIN_MV) /
                                      (BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0f);
    }

    if (out_percentage != NULL) {
        *out_percentage = s_cached_battery_pct;
    }

    return ret;
}

esp_err_t battery_get_percentage(int *out_percentage) {
    esp_err_t ret = ESP_OK;

    if (s_cached_battery_pct < 0) {
        return battery_update_cache(out_percentage);
    }
    if (out_percentage != NULL) {
        *out_percentage = s_cached_battery_pct;
    }

    return ret;
}
