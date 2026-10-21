#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <string.h>
#include "esp_err.h"
#include "wifi_manager.h"
#include "esp_http_client.h"
#include "audio_manager.h" // Importas el gestor de audio para pedirle los datos
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "audio_bsp/audio_bsp.h"
#include "cJSON.h"
#include <mbedtls/base64.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_AUDIO_BUF_SIZE  (30 * 16000 * 2) // Ejemplo de buffer (30s aprox)

const char* api_client_get_last_cmd_output(void);
const char* api_client_get_remote_cmd_output(void);
esp_err_t api_client_send_audio(char *out_msg, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // API_CLIENT_H
