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

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID           "YOUR WIFI SSID"
#define WIFI_PASS           "YOUR WIFI PASSWORD"

#define MAXIMUM_RETRY       5
#define WIFI_TIMEOUT_MS     12000
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

esp_err_t wifi_system_init(void);
esp_err_t wifi_connect(void);
esp_err_t wifi_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
