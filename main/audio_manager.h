#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdint.h>
#include "esp_err.h"
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EPD_PWR_PIN
#define EPD_PWR_PIN GPIO_NUM_6
#endif
#define Audio_PWR_PIN   GPIO_NUM_42
#define VBAT_PWR_PIN    GPIO_NUM_17

// Internal audio state variables and buffers
#define MAX_AUDIO_BUF_SIZE     (30 * 16000 * 2) // Example buffer (~30 seconds at 16kHz 16-bit)
#define CHUNK_SIZE             2048
#define BYTES_PER_SEC          (16000 * 2)      // Adjust according to sample rate and channels (e.g. 16kHz, 16-bit)

#define INITIAL_AUDIO_VOLUME 80  // Default volume level (0 to 100)

esp_err_t audio_manager_init(void);
esp_err_t audio_manager_toggle_recording(void);
bool audio_manager_is_recording(void);
size_t audio_manager_get_recorded_bytes(void);
uint8_t* audio_manager_get_audio_ptr(void);
size_t audio_manager_get_recorded_bytes(void);
esp_err_t set_audio_volume(uint8_t vol);
esp_err_t audio_sync_volume_nvs_automatically(void);
esp_err_t audio_rtc_cache_init(void);
esp_err_t audio_get_volume(uint8_t *out_vol);
esp_err_t audio_set_and_save_volume(uint8_t vol);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_MANAGER_H
