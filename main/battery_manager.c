#include "battery_manager.h"

static const char *TAG = "BATTERY_MANAGER";

// Static cache for battery percentage (-1 indicates uninitialized)
static int s_cached_battery_pct = -1;

/**
 * @brief Measures actual voltage via ADC and returns it through an output pointer.
 * Handles power divider GPIO enablement, ADC initialization, calibration, sampling, and resource cleanup.
 * 
 * @param out_voltage_mv Pointer to store the measured voltage in millivolts.
 * @return esp_err_t ESP_OK on success, or a specific error code on failure.
 */
esp_err_t battery_get_voltage_mv(uint32_t *out_voltage_mv)
{
    esp_err_t ret = ESP_OK;
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_cali_handle_t cali_handle = NULL;
    bool cali_enabled = false;
    
    if (out_voltage_mv == NULL) {
        ESP_LOGW(TAG, "Invalid output pointer provided for battery voltage");
        return ESP_ERR_INVALID_ARG;
    }

    /* STEP 1: Enable power via GPIO if defined for voltage divider */
#ifdef BATTERY_PWR_EN_GPIO
    ret = gpio_reset_pin(BATTERY_PWR_EN_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset battery power GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = gpio_set_direction(BATTERY_PWR_EN_GPIO, GPIO_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set battery power GPIO direction: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = gpio_set_level(BATTERY_PWR_EN_GPIO, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set battery power GPIO level: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Stabilization delay
#endif

    /* STEP 2: Reset ADC pin to remove any Deep Sleep isolation */
    ret = gpio_reset_pin(BATTERY_ADC_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to reset ADC GPIO: %s", esp_err_to_name(ret));
        // Non-fatal, continue execution
    }

    /* STEP 3: Initialize ADC Unit */
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC unit: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /* STEP 4: Configure ADC Channel */
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = BATTERY_ADC_ATTEN,
    };
    ret = adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /* STEP 5: Calibration Scheme Setup */
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t cali_err = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if (cali_err == ESP_OK) {
        cali_enabled = true;
    } else {
        ESP_LOGW(TAG, "Curve fitting calibration not supported/failed (%s), falling back to linear calculation", esp_err_to_name(cali_err));
    }

    /* STEP 6: Sample Readings */
    int raw_sum = 0;
    const int samples = 8;
    for (int i = 0; i < samples; i++) {
        int raw = 0;
        ret = adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &raw);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read ADC value at sample %d: %s", i, esp_err_to_name(ret));
            goto cleanup;
        }
        raw_sum += raw;
    }
    int raw_avg = raw_sum / samples;

    ESP_LOGD(TAG, "ADC Raw Average Reading: %d", raw_avg);

    /* STEP 7: Convert to mV */
    int voltage_adc_mv = 0;
    if (cali_enabled) {
        adc_cali_raw_to_voltage(cali_handle, raw_avg, &voltage_adc_mv);
    } else {
        voltage_adc_mv = (raw_avg * 3300) / 4095;
    }

    *out_voltage_mv = (uint32_t)(voltage_adc_mv * VOLTAGE_DIVIDER_RATIO);
    ret = ESP_OK;

cleanup:
    /* STEP 8: Resource Cleanup */
    if (cali_enabled && cali_handle != NULL) {
        adc_cali_delete_scheme_curve_fitting(cali_handle);
    }
    if (adc_handle != NULL) {
        adc_oneshot_del_unit(adc_handle);
    }

    /* Turn off voltage divider power GPIO if enabled */
#ifdef BATTERY_PWR_EN_GPIO
    gpio_set_level(BATTERY_PWR_EN_GPIO, 0);
#endif

    return ret;
}

/**
 * @brief Measures actual voltage via ADC, maps it to a percentage, and updates the cache variable.
 * Must be called only on boot or wake up in app_main().
 * 
 * @param out_percentage Pointer to store the updated battery percentage.
 * @return esp_err_t ESP_OK on success, or a specific error code.
 */
esp_err_t battery_update_cache(int *out_percentage)
{
    esp_err_t ret = ESP_OK;
    uint32_t voltage_mv = 0;

    ret = battery_get_voltage_mv(&voltage_mv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to obtain battery voltage for cache update");
        return ret;
    }

    if (voltage_mv >= BATTERY_MAX_MV) {
        s_cached_battery_pct = 100;
    } else if (voltage_mv <= BATTERY_MIN_MV) {
        s_cached_battery_pct = 0;
    } else {
        /* Simple linear mapping between MIN and MAX */
        s_cached_battery_pct = (int)(((float)(voltage_mv - BATTERY_MIN_MV) /
                                       (BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0f);
    }

    ESP_LOGI(TAG, "Battery updated via ADC: %lu mV (%d%%)", (unsigned long)voltage_mv, s_cached_battery_pct);

    if (out_percentage != NULL) {
        *out_percentage = s_cached_battery_pct;
    }

    return ret;
}

/**
 * @brief Returns the cached percentage value without activating the ADC or Power Gate.
 * If never measured, performs an initial safety measurement.
 *
 * @param out_percentage Pointer to store the retrieved battery percentage.
 * @return esp_err_t ESP_OK on success, or a specific error code.
 */
esp_err_t battery_get_percentage(int *out_percentage)
{
    esp_err_t ret = ESP_OK;

    if (out_percentage == NULL) {
        ESP_LOGW(TAG, "Invalid output pointer provided for battery percentage");
        return ESP_ERR_INVALID_ARG;
    }

    /* If for any reason the cache has not been initialized, perform measurement once */
    if (s_cached_battery_pct < 0) {
        ESP_LOGI(TAG, "Battery cache uninitialized. Performing initial measurement...");
        return battery_update_cache(out_percentage);
    }

    ESP_LOGD(TAG, "Battery (cached): %d%%", s_cached_battery_pct);
    *out_percentage = s_cached_battery_pct;

    return ret;
}
