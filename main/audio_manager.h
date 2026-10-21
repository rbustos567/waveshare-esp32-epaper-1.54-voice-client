#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdint.h>
#include "esp_err.h"
//#include "esp_log.h"
//#include "driver/gpio.h"
//#include "audio_bsp.h"
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"


#ifdef __cplusplus
extern "C" {
#endif

#define EPD_PWR_PIN     GPIO_NUM_6
#define Audio_PWR_PIN   GPIO_NUM_42
#define VBAT_PWR_PIN    GPIO_NUM_17

esp_err_t audio_manager_init(void);

// Control de grabación (Iniciar / Detener / Toggle)
void audio_manager_toggle_recording(void);
bool audio_manager_is_recording(void);
size_t audio_manager_get_recorded_bytes(void);
void audio_manager_play_recorded_audio(void);
uint8_t* audio_manager_get_audio_ptr(void);
size_t audio_manager_get_recorded_bytes(void);
void audio_manager_start_recording(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_MANAGER_H
