#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <string.h>
#include "esp_err.h"
#include "wifi_manager.h"
#include "esp_http_client.h"
#include "audio_manager.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "audio_bsp/audio_bsp.h"
#include "cJSON.h"
#include <mbedtls/base64.h>

#ifdef __cplusplus
extern "C" {
#endif

#define API_C_CMD_OUT_SIZE           1024
#define API_C_REMOTE_CMD_OUT_SIZE    256
#define MAX_AUDIO_BUF_SIZE  (30 * 16000 * 2) // Buffer size for aprox. 30 sec of recording

// IMPORTANT: Need to add here your server ip address
//#define API_C_SERVER_URL      "http://<YOUR SERVER IP ADDRESS>:8000/process-audio"
//#define API_C_SERVER_URL      "https://orangepi6plus-1.tail6a8895.ts.net/process-audio" 
#define API_C_SERVER_URL      "http://192.168.1.36:8000/process-audio"

#define API_C_SERVER_TIMEOUT  30000

#define API_C_SERVER_KEY         "API-Key"      
#define API_C_SERVER_PASS	 "password123x"

const char* api_client_get_last_cmd_output(void);
const char* api_client_get_remote_cmd_output(void);
esp_err_t api_client_send_audio(char *out_msg, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // API_CLIENT_H
