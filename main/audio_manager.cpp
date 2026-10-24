#include "audio_manager.h"
#include "audio_bsp.h"
#include "board_power_bsp.h"
#include "driver/gpio.h"
#include "Config/DEV_Config.h"
			      
static const char *TAG = "AUDIO_MANAGER";

board_power_bsp_t board_div(EPD_PWR_PIN,Audio_PWR_PIN,VBAT_PWR_PIN);

// Internal audio state variables and buffers
#define MAX_AUDIO_BUF_SIZE     (30 * 16000 * 2) // Example buffer (~30 seconds at 16kHz 16-bit)
#define CHUNK_SIZE             2048
#define BYTES_PER_SEC          (16000 * 2)      // Adjust according to sample rate and channels (e.g. 16kHz, 16-bit)

static uint8_t *s_audio_ptr = NULL;
static volatile bool s_is_recording = false;
static size_t s_actual_recorded_bytes = 0;
static TaskHandle_t s_record_task_handle = NULL;
static uint8_t *s_rx_audio_ptr = NULL;
static size_t s_rx_recorded_bytes = 0;

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
