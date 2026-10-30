#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <sys/time.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "ui_menu_manager.h"
#include "ui_renderer.h"
#include "ui_control.h"
#include "wifi_manager.h"
#include "power_manager.h"
#include "time_manager.h"
#include "battery_manager.h"
#include "driver/gpio.h"
#include "audio_manager.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void) {

    esp_err_t ret = ESP_OK;

    /* ======================================================= */
    /* Code block to allow running on battery using LiPo power */
    /* ======================================================= */
    /* Definition of energy retention pin on Waveshare ESP32-S3-ePaper-1.54G */
    #define POWER_HOLD_PIN GPIO_NUM_17
    /* Configure GPIO17 as output to keep the power circuit open/latched */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << POWER_HOLD_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    /* Set HIGH to latch/retain battery/external power */
    gpio_set_level(POWER_HOLD_PIN, 1);
    /* ======================================================== */
    /* End of code to allow running on battery using LiPo power */
    /* ======================================================== */

    /* STARTUP */
    /* Release any hold state or residual GPIO isolation from Deep Sleep */
    gpio_deep_sleep_hold_dis();

    /* 1. INITIALIZE NVS FLASH (Required for reading/storing persistence) */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	ESP_LOGI(TAG, "Erasing NVS flash due to version mismatch or full pages.");
        ESP_ERROR_CHECK(nvs_flash_erase()); /* Safe only during critical boot partition recovery */
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        // Non-fatal for UI, but persistent storage won't work
    }

    /* 2. INITIALIZE CORE MANAGERS */
    ret = time_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Time manager initialization warning: %s", esp_err_to_name(ret));
    }
    
    ret = battery_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Initialization of battery measurement failed: %s", esp_err_to_name(ret));
    }

    ret = power_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Power manager initialization warning: %s", esp_err_to_name(ret));
    }

    ret = audio_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio manager initialization warning: %s", esp_err_to_name(ret));
    }

    /* 3. IDENTIFY WAKEUP CAUSE */
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
	/* ======================================================= */
        /* FIRST COLD BOOT (Power-On / Reset Button / Cable Plugged) */
        /* ======================================================= */
        ESP_LOGI(TAG, "System booted normally (Power-on Reset / Soft Reset).");
        ret = wifi_sync_credentials_nvs_automatically();
	if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Wifi credentials sync failed : %s", esp_err_to_name(ret));
        }

	ret = wifi_rtc_cache_init();
	if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Wifi RTC caching failed : %s", esp_err_to_name(ret));
        }

    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
	/* ======================================================= */
        /* WAKE UP FROM DEEP SLEEP (BOOT Button Pressed)           */
        /* ======================================================= */
        ESP_LOGI(TAG, "Woken up from Deep Sleep by BOOT button (GPIO0)!");

	/* Wait for the user to release the boot button */
        gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
        while (gpio_get_level(GPIO_NUM_0) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

	ESP_LOGI(TAG, "BOOT button released. Proceeding to normal UI execution.");
    }

    /* Always try to synchronize Network Clock (NTP/SNTP) to mitigate
     * internal RC oscillator drift and thermal variance across wakeups. */
    ret = wifi_system_init();
    if (ret == ESP_OK) {
        if (wifi_connect() == ESP_OK) {
            ret = time_manager_sync_sntp(5000);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "SNTP time synchronization failed: %s", esp_err_to_name(ret));
            }
            wifi_disconnect();
        } else {
            ESP_LOGW(TAG, "Failed to connect to Wi-Fi for NTP synchronization during boot");
        }
    } else {
        ESP_LOGW(TAG, "Wi-Fi system initialization failed: %s", esp_err_to_name(ret));
    }

    ret = uptime_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Uptime manager initialization warning: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Main execution initializing...");

    /* ======================================================= */
    /* 4. MAIN EXECUTION (Splash Screen / Menu)                */
    /* ======================================================= */
    ESP_LOGI(TAG, "%s: Starting application...", __func__);

    /* Critical UI components initialization with clean error handling */
    ret = init_menu();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize menu structure: %s", esp_err_to_name(ret));
    }

    ret = render_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize EPD render driver: %s", esp_err_to_name(ret));
    }

    ret = render_full("RUNNING", NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Initial full render warning: %s", esp_err_to_name(ret));
    }

    ret = ui_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UI controller/tasks: %s", esp_err_to_name(ret));
    }
}
