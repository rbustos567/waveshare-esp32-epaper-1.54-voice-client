#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EPD_PWR_PIN
#define EPD_PWR_PIN GPIO_NUM_6
#endif
#define Audio_PWR_PIN   GPIO_NUM_42
#define VBAT_PWR_PIN    GPIO_NUM_17

esp_err_t audio_manager_init(void);
esp_err_t audio_manager_toggle_recording(void);
bool audio_manager_is_recording(void);
size_t audio_manager_get_recorded_bytes(void);
uint8_t* audio_manager_get_audio_ptr(void);
size_t audio_manager_get_recorded_bytes(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_MANAGER_H
