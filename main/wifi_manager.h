#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_err.h"
#include "wifi_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAXIMUM_RETRY       5
#define WIFI_TIMEOUT_MS     12000
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_WIFI_PROFILES   5  // Maximum number of profiles supported in RTC storage

// Structure representing a single Wi-Fi credential pair
typedef struct {
    char ssid[32];
    char pass[64];
} wifi_credential_t;

esp_err_t wifi_system_init(void);
esp_err_t wifi_connect(void);
esp_err_t wifi_disconnect(void);
esp_err_t wifi_sync_credentials_nvs_automatically(void);
esp_err_t wifi_rtc_cache_init(void);
esp_err_t wifi_get_active_credentials(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
