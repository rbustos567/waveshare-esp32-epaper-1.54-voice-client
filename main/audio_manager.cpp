#include "audio_manager.h"
#include "audio_bsp.h"
#include "board_power_bsp.h"
#include "driver/gpio.h"
#include "Config/DEV_Config.h" // Para reutilizar los defines existentes si gustas
			       //
static const char *TAG = "audio_manager";

board_power_bsp_t board_div(EPD_PWR_PIN,Audio_PWR_PIN,VBAT_PWR_PIN);

// Variables de estado y buffers internos de audio
#define MAX_AUDIO_BUF_SIZE  (30 * 16000 * 2) // Ejemplo de buffer (30s aprox)
#define CHUNK_SIZE          2048
#define BYTES_PER_SEC (16000 * 2) // Ajusta según tu tasa de muestreo y canales (ej. 16kHz, 16-bit)

static uint8_t *s_audio_ptr = NULL; 
static volatile bool s_is_recording = false;
static size_t s_actual_recorded_bytes = 0;
static TaskHandle_t s_record_task_handle = NULL;
static uint8_t *s_rx_audio_ptr = NULL;
static size_t s_rx_recorded_bytes = 0;

// Tarea privada que corre en segundo plano (Core 1)
static void record_task(void *pvParameters) {

    size_t chunk_to_read = CHUNK_SIZE;
    memset(s_audio_ptr, 0, MAX_AUDIO_BUF_SIZE);

    ESP_LOGI(TAG, "Iniciando captura de audio...");

    while (s_is_recording && (s_actual_recorded_bytes < MAX_AUDIO_BUF_SIZE)) {
        if (s_actual_recorded_bytes + chunk_to_read > MAX_AUDIO_BUF_SIZE) {
            chunk_to_read = MAX_AUDIO_BUF_SIZE - s_actual_recorded_bytes;
        }
        
        // Lectura desde el códec/I2S
        // audio_playback_read(...) o la función que uses para leer el mic
        audio_playback_read(s_audio_ptr + s_actual_recorded_bytes, chunk_to_read);
        s_actual_recorded_bytes += chunk_to_read;
        
        vTaskDelay(pdMS_TO_TICKS(1)); // Cede control para evitar Watchdog
    }

    s_is_recording = false;
    ESP_LOGI(TAG, "Grabación finalizada. Total: %d bytes", s_actual_recorded_bytes);

    s_record_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_manager_init(void) {
    esp_err_t ret = ESP_OK;

    // 1. Encender energía de audio mediante el BSP
    board_div.POWEER_Audio_ON();
//    vTaskDelay(pdMS_TO_TICKS(50));

    // 2. Verificar y reservar memoria en PSRAM
    size_t spiram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t spiram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "PSRAM Total: %u bytes | PSRAM Libre: %u bytes", spiram_total, spiram_free);

    if (spiram_total == 0) {
        ESP_LOGE(TAG, "¡Atención! La PSRAM no está detectada o no está habilitada en sdkconfig.");
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Reservar buffer de grabación
    s_audio_ptr = (uint8_t *)heap_caps_malloc(MAX_AUDIO_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (s_audio_ptr == NULL) {
        ESP_LOGE(TAG, "Error al asignar %d bytes en PSRAM para s_audio_ptr", MAX_AUDIO_BUF_SIZE);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Buffer s_audio_ptr con %d bytes asignados en PSRAM correctamente", MAX_AUDIO_BUF_SIZE);

    // Reservar buffer de recepción de red/servidor (si lo sigues usando)
    s_rx_audio_ptr = (uint8_t *)heap_caps_malloc(MAX_AUDIO_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (s_rx_audio_ptr == NULL) {
        ESP_LOGE(TAG, "Error al asignar %d bytes en PSRAM para s_rx_audio_ptr", MAX_AUDIO_BUF_SIZE);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Buffer s_rx_audio_ptr con %d bytes asignados en PSRAM correctamente", MAX_AUDIO_BUF_SIZE);

    audio_bsp_init();
    audio_play_init();

 //   board_div.POWEER_Audio_OFF();
 //   ESP_LOGI(TAG, "Hardware de audio apagado para ahorrar energía.");

    return ret;
}

void audio_manager_toggle_recording(void) {
    if (s_is_recording) {
        ESP_LOGI(TAG, "Solicitando detener grabación...");
        s_is_recording = false; // El while de la tarea se detiene solo
    } else {
        ESP_LOGI(TAG, "Empezando grabación...");
        s_is_recording = true;
        s_actual_recorded_bytes = 0;

        xTaskCreatePinnedToCore(
            record_task,
            "record_task",
	    4096,
            NULL,
            5,
            &s_record_task_handle,
            1
        );
    }
}

void audio_manager_play_recorded_audio(void) {
    if (s_audio_ptr != NULL && s_actual_recorded_bytes > 0) {
        ESP_LOGI(TAG, "Reproduciendo %d bytes (%.2f segundos)...", 
                 s_actual_recorded_bytes, (float)s_actual_recorded_bytes / BYTES_PER_SEC);
       
//        board_div.POWEER_Audio_ON();
//        vTaskDelay(pdMS_TO_TICKS(100));	
        // Reproduce únicamente los bytes capturados reales
        audio_playback_write(s_audio_ptr, s_actual_recorded_bytes);

//	vTaskDelay(pdMS_TO_TICKS(100));
//	board_div.POWEER_Audio_OFF();
//        ESP_LOGI(TAG, "Hardware de audio apagado para ahorrar energía.");
    } else {
        ESP_LOGW(TAG, "No hay audio grabado para reproducir");
    }
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

void audio_manager_start_recording(void) {
    if (!s_is_recording) {
        s_is_recording = true;
        s_actual_recorded_bytes = 0;
        xTaskCreatePinnedToCore(record_task, "record_task", 4096, NULL, 5, &s_record_task_handle, 1);
    }
}
