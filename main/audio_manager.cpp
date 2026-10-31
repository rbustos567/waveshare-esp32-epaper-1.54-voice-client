#include "audio_manager.h"
#include "audio_bsp.h"
#include "board_power_bsp.h"
#include "driver/gpio.h"
#include "Config/DEV_Config.h"
			      
static const char *TAG = "AUDIO_MANAGER";

board_power_bsp_t board_div(EPD_PWR_PIN,Audio_PWR_PIN,VBAT_PWR_PIN);

static uint8_t *s_audio_ptr = NULL;
static volatile bool s_is_recording = false;
static size_t s_actual_recorded_bytes = 0;
static TaskHandle_t s_record_task_handle = NULL;
static uint8_t *s_rx_audio_ptr = NULL;
static size_t s_rx_recorded_bytes = 0;

// RTC RAM storage structure for audio volume (survives deep sleep and fast reboots)
RTC_DATA_ATTR struct {
    uint8_t volume;
    bool is_initialized;
} rtc_audio_storage;

/**
 * @brief Background task running on Core 1 for audio recording.
 */
static void record_task(void *pvParameters)
{
    size_t chunk_to_read = CHUNK_SIZE;
    
    if (s_audio_ptr != NULL) {
        memset(s_audio_ptr, 0, MAX_AUDIO_BUF_SIZE);
    }

    ESP_LOGI(TAG, "Starting audio capture...");

    while (s_is_recording && (s_actual_recorded_bytes < MAX_AUDIO_BUF_SIZE)) {
        if (s_actual_recorded_bytes + chunk_to_read > MAX_AUDIO_BUF_SIZE) {
            chunk_to_read = MAX_AUDIO_BUF_SIZE - s_actual_recorded_bytes;
        }

        /* Read from codec/I2S */
        audio_playback_read(s_audio_ptr + s_actual_recorded_bytes, chunk_to_read);
        s_actual_recorded_bytes += chunk_to_read;

        vTaskDelay(pdMS_TO_TICKS(1)); // Yield control to prevent watchdog trigger
    }

    s_is_recording = false;
    ESP_LOGI(TAG, "Recording finished. Total: %u bytes", (unsigned int)s_actual_recorded_bytes);

    s_record_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_manager_init(void) {
    esp_err_t ret = ESP_OK;

    /* 1. Turn on audio power via BSP */
    board_div.POWEER_Audio_ON();

    /* 2. Verify and allocate memory in PSRAM */
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "PSRAM Total: %u bytes | PSRAM Free: %u bytes", (unsigned int)psram_total, (unsigned int)psram_free);

    if (psram_total == 0) {
        ESP_LOGE(TAG, "Warning! PSRAM not detected or not enabled in sdkconfig.");
        board_div.POWEER_Audio_OFF();
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Allocate recording buffer */
    s_audio_ptr = (uint8_t *)heap_caps_malloc(MAX_AUDIO_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (s_audio_ptr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes in PSRAM for s_audio_ptr", (unsigned int)MAX_AUDIO_BUF_SIZE);
        board_div.POWEER_Audio_OFF();
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Buffer s_audio_ptr successfully allocated with %u bytes in PSRAM", (unsigned int)MAX_AUDIO_BUF_SIZE);

    /* Allocate network/server RX buffer */
    s_rx_audio_ptr = (uint8_t *)heap_caps_malloc(MAX_AUDIO_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (s_rx_audio_ptr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes in PSRAM for s_rx_audio_ptr", (unsigned int)MAX_AUDIO_BUF_SIZE);
        free(s_audio_ptr);
        s_audio_ptr = NULL;
        board_div.POWEER_Audio_OFF();
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Buffer s_rx_audio_ptr successfully allocated with %u bytes in PSRAM", (unsigned int)MAX_AUDIO_BUF_SIZE);

    audio_bsp_init();
    audio_play_init();

    ESP_LOGI(TAG, "Audio hardware powered off to save energy.");

    return ret;
}

/**
 * @brief Starts the recording process by spawning the background task.
 */
esp_err_t audio_manager_toggle_recording(void) {

    esp_err_t ret = ESP_OK;

    if (s_is_recording) {
        ESP_LOGD(TAG, "Requesting to stop recording...");
        s_is_recording = false;
    } else {
        ESP_LOGD(TAG, "Starting to record...");
        s_is_recording = true;
        s_actual_recorded_bytes = 0;

        BaseType_t task_created = xTaskCreatePinnedToCore(
            record_task,
            "record_task",
	    4096,
            NULL,
            5,
            &s_record_task_handle,
            1
        );

	if (task_created != pdPASS) {
            s_is_recording = false;
            ESP_LOGE(TAG, "Failed to create record_task");
            return ESP_FAIL;
        }
    }

    return ret;
}

bool audio_manager_is_recording(void) {
    
    return s_is_recording;
}

uint8_t* audio_manager_get_audio_ptr(void) {
    
    return s_audio_ptr;
}

size_t audio_manager_get_recorded_bytes(void) {

    return s_actual_recorded_bytes;
}

esp_err_t set_audio_volume(uint8_t vol) {
    esp_err_t ret = ESP_OK;

    if (vol > 100) {
        ESP_LOGE(TAG, "Volume out of range (0 - 100): %u", vol);
        return ESP_ERR_INVALID_ARG;
    }

    audio_playback_set_vol(vol);
    
    ESP_LOGI(TAG, "Volume set to: %u%%", vol);

    return ret;
}

/**
 * @brief Automatically synchronizes the audio volume with NVS storage.
 *        If no volume key exists in NVS, it writes the default header value.
 */
esp_err_t audio_sync_volume_nvs_automatically(void) {
    esp_err_t ret = ESP_OK;
    nvs_handle_t nvs_h;
    
    ret = nvs_open("storage", NVS_READWRITE, &nvs_h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return ret;
    }

    int32_t stored_vol = -1;
    ret = nvs_get_i32(nvs_h, "audio_vol", &stored_vol);

    // If the volume key does not exist in NVS, save the default value from header
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No volume found in NVS. Saving default: %d", INITIAL_AUDIO_VOLUME);
        ret = nvs_set_i32(nvs_h, "audio_vol", INITIAL_AUDIO_VOLUME);
        if (ret == ESP_OK) {
            nvs_commit(nvs_h);
        }
    }

    nvs_close(nvs_h);
    
    return ret;
}

/**
 * @brief Caches the audio volume from NVS into RTC RAM during cold boot.
 */
esp_err_t audio_rtc_cache_init(void) {
    esp_err_t ret = ESP_OK;
    nvs_handle_t nvs_h;
    
    ret = nvs_open("storage", NVS_READONLY, &nvs_h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return ret;
    }

    int32_t stored_vol = INITIAL_AUDIO_VOLUME;
    nvs_get_i32(nvs_h, "audio_vol", &stored_vol);
    nvs_close(nvs_h);

    rtc_audio_storage.volume = (uint8_t)stored_vol;
    rtc_audio_storage.is_initialized = true;

    ESP_LOGI(TAG, "Audio volume cached in RTC: %u%%", rtc_audio_storage.volume);
    
    return ret;
}
/**
 * @brief Gets the current volume instantly from RTC RAM (zero latency, zero NVS wear).
 */
esp_err_t audio_get_volume(uint8_t *out_vol) {
    esp_err_t ret = ESP_OK;

    if (out_vol == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!rtc_audio_storage.is_initialized) {
        *out_vol = INITIAL_AUDIO_VOLUME;

        return ESP_ERR_INVALID_STATE;
    }

    *out_vol = rtc_audio_storage.volume;

    return ret;
}

/**
 * @brief Updates the volume in RTC RAM, applies it to the hardware codec,
 *        and permanently saves it to NVS.
 */
esp_err_t audio_set_and_save_volume(uint8_t vol) {
    esp_err_t ret = ESP_OK;

    // Validate that the volume is within the valid range (0 - 100)
    if (vol > 100) {
        ESP_LOGE(TAG, "Volume out of range (0-100): %u", vol);

        return ESP_ERR_INVALID_ARG;
    }

    // 1. Update in RTC RAM for instant UI feedback
    rtc_audio_storage.volume = vol;

    // 2. Apply volume dynamically to the audio hardware via wrapper
    audio_playback_set_vol(vol);

    // 3. Persist permanently in NVS to survive power cycles
    nvs_handle_t nvs_h;
    ret = nvs_open("storage", NVS_READWRITE, &nvs_h);
    if (ret == ESP_OK) {
        nvs_set_i32(nvs_h, "audio_vol", (int32_t)vol);
        nvs_commit(nvs_h);
        nvs_close(nvs_h);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS namespace");

	return ret;
    }

    ESP_LOGI(TAG, "Volume updated, applied, and saved to NVS: %u%%", vol);
    
    return ret;
}
