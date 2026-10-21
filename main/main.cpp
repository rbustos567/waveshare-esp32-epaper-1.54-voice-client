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

#define BUTTON_TO_TEST GPIO_NUM_0 // Test GPIO0 (BOOT)
static const char *TAG = "MAIN";

extern "C" void app_main(void) {

// WORKS	
//    ESP_LOGI(TAG, "%s: Starting application...", __func__);

//    ESP_ERROR_CHECK(wifi_system_init());
//    ESP_ERROR_CHECK(init_menu());
//    ESP_ERROR_CHECK(render_init());
//    ESP_ERROR_CHECK(render_splash_to_buffer());
//    ESP_ERROR_CHECK(render_full());
//    ESP_ERROR_CHECK(ui_control_init());

//    if (wifi_connect() == ESP_OK) {
//        wifi_disconnect();
//    }
// END WORKS


    // INICIO
    // Liberar cualquier estado de "hold" o aislamiento residual de GPIOs del Deep Sleep
    gpio_deep_sleep_hold_dis();

    // 1. INICIALIZAR NVS FLASH (Requisito para leer/guardar persistencia)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	ESP_LOGI(TAG, "Erasing nvs flash.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. RECUPERAR / APLICAR TIMEZONE DESDE NVS
    // Reemplaza time_manager_set_timezone(LOCAL_TIME_ZONE); por la función de init
    time_manager_init(); 

    uptime_manager_init();
    battery_update_cache();

    // Iniciar el timer de inactividad
    power_manager_init();

    // Inicializar buses compartidos (I2C / SPI)
    audio_manager_init();
    // i2c_master_init();
    // spi_bus_init();

    // 3. IDENTIFICAR LA CAUSA DEL ARRANQUE
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        /* 
         * =========================================================
         * FIRST COLD BOOT (Power-On / Reset Button / Cable Conectado)
         * =========================================================
         */
        ESP_LOGI(TAG, "System booted normally (Power-on Reset / Soft Reset).");

        // a) Sincronizar Reloj por Red (NTP/SNTP)
        ESP_ERROR_CHECK(wifi_system_init());
        if (wifi_connect() == ESP_OK) {
            time_manager_sync_sntp(5000);
            wifi_disconnect();
        }

        // b) Inicializar Codec de Audio completo (I2S + I2C ES8311)

        // c) Inicializar Pantalla completo (Full Refresh / Clear Frame)
        // display_init_full();

    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        /* 
         * =========================================================
         * WAKEUP FROM DEEP SLEEP (Presión del Botón BOOT)
         * =========================================================
         */
        ESP_LOGI(TAG, "Woken up from Deep Sleep by BOOT button (GPIO0)!");

        // Esperar a que el usuario libere el botón de arranque
        gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
        while (gpio_get_level(GPIO_NUM_0) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        ESP_LOGI(TAG, "BOOT button released. Proceeding to normal UI execution.");
        // a) NO inicializar Wi-Fi (el RTC interno ya tiene la hora).
        // b) Mantener el Codec de Audio en modo Power-Down / Off.
        // c) Inicialización rápida de pantalla (Partial Refresh solo para el reloj).
        // display_init_fast_partial();
    }

    ESP_LOGI(TAG, "Main execution...");
    // 3. EJECUCIÓN PRINCIPAL (Splash Screen / Menú)
    ESP_LOGI(TAG, "%s: Starting application...", __func__);


    ESP_ERROR_CHECK(init_menu());
    ESP_ERROR_CHECK(render_init());
    //ESP_ERROR_CHECK(render_splash_to_buffer("RUNNING"));
    ESP_ERROR_CHECK(render_full("RUNNING", NULL));
    ESP_ERROR_CHECK(ui_control_init());

    char time_str[16]; // Para "19:05:30"
    char date_str[32]; // Para "2026-10-05"

    // Obtener la hora formateada
    time_manager_get_formatted_time(time_str, sizeof(time_str), "%H:%M:%S");

    // Obtener la fecha formateada
    time_manager_get_formatted_time(date_str, sizeof(date_str), "%Y-%m-%d");

    ESP_LOGI(TAG, "--------------------------------");
    ESP_LOGI(TAG, " Fecha actual: %s", date_str);
    ESP_LOGI(TAG, " Hora actual:  %s", time_str);
    ESP_LOGI(TAG, "--------------------------------");

    int battery_pct = battery_get_percentage();

    ESP_LOGI(TAG, "Porcentaje de batería: %d%%", battery_pct);


    char uptime_str[32];
    uptime_get_formatted(uptime_str, sizeof(uptime_str));
    ESP_LOGI("MAIN", "Uptime actual: %s", uptime_str);

    // 4. INICIAR CONTRARRELOJ DE INACTIVIDAD (5 minutos -> Deep Sleep)
    // start_inactivity_timer();

//    vTaskDelay(pdMS_TO_TICKS(INACTIVITY_TIMEOUT_SEC * 1000));
//    uptime_prepare_for_sleep();
//    enter_deep_sleep_boot_button();
}
