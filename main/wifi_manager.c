#include "wifi_manager.h"

static const char *TAG = "WIFI_MANAGER";

// FreeRTOS event group handle to signal when connected or failed
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/**
 * @brief Event handler for Wi-Fi and IP events.
 * Executes asynchronously in the background when network state changes occur.
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    // Event: Wi-Fi driver started successfully
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
	ESP_LOGI(TAG, "Wi-Fi station driver started. Initiating connection...");
        esp_wifi_connect();
    } 
    // Event: Disconnected from Access Point or connection attempt failed
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            s_retry_num++;
	    ESP_LOGW(TAG, "Wi-Fi disconnected. Retrying connection (%d/%d)...", s_retry_num, MAXIMUM_RETRY);
	    esp_wifi_connect();
        } else {
            // Reached max retries; notify main task via FreeRTOS bit flag
	    ESP_LOGW(TAG, "Max connection retries (%d) reached. Signaling failure.", MAXIMUM_RETRY);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } 
    // Event: Connected to AP and successfully obtained an IP address via DHCP
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Successfully obtained IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        // Notify main task of successful connection
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Global Wi-Fi Subsystem Initialization.
 * Call this ONLY ONCE during system startup (e.g., in app_main) before using any network functions.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_system_init(void) 
{
    esp_err_t ret = ESP_OK;

    /*
     * STEP 1: Initialize Non-Volatile Storage (NVS)
     * Wi-Fi driver requires NVS to store system configurations and PHY calibration data.
     */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash full or new version found, erasing and re-initializing...");
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        return ret;
    }

    /* STEP 2: Initialize Underlying TCP/IP Stack & Event Loop */
    /* Creates the core netif abstraction layer and default system event loop task. */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize underlying TCP/IP stack (esp_netif): %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create default system event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Creates default Wi-Fi station network interface (Note: esp_netif_create_default_wifi_sta returns a pointer, so we check if it's NULL)
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default Wi-Fi station network interface");
        return ESP_FAIL;
    }

    /*
     * STEP 3: Allocate Wi-Fi Driver Control Blocks
     * Initializes internal memory buffers and hardware configuration using default parameters.
     */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi driver control blocks: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Wi-Fi system core initialized successfully.");
    return ESP_OK;
}

/**
 * @brief Connects to the configured Wi-Fi AP and blocks until connection completes or times out.
 * Can be called multiple times during runtime (e.g., periodic wakeups for e-Paper updates).
 * 
 * @return esp_err_t ESP_OK on success, ESP_FAIL on auth/connection fail, ESP_ERR_TIMEOUT on timeout.
 */
esp_err_t wifi_connect(void) 
{
    esp_err_t ret = ESP_OK;
    esp_event_handler_instance_t instance_any_id = NULL;
    esp_event_handler_instance_t instance_got_ip = NULL;
    /*
     * STEP 1: Create FreeRTOS Event Group for Synchronization
     * Creates an event group instance to block the caller task until connection events occur.
     */
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Error when creating Wi-Fi Event Group");
        return ESP_ERR_NO_MEM;
    }

    s_retry_num = 0; // Reset retry counter prior to connection attempt
    /*
     * STEP 2: Register Event Callbacks
     * Attaches event_handler callback to listen for Wi-Fi lifecycle and IP assignment events.
     */
    ret = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &instance_any_id
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &event_handler,
        NULL,
        &instance_got_ip
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /*
     * STEP 3: Configure Target Network Parameters
     */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            },
        },
    };

    /*
     * STEP 4: Start Wi-Fi Radio Interface
     * Sets mode to Station, loads config, and powers on RF radio.
     * Triggers WIFI_EVENT_STA_START in background which triggers esp_wifi_connect().
     */
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Wi-Fi mode: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Wi-Fi config: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Starting connection to SSID: %s...", WIFI_SSID);

    /*
     * STEP 5: Synchronous Wait (Block Task Execution)
     * Suspends caller task until event_handler sets WIFI_CONNECTED_BIT / WIFI_FAIL_BIT,
     * or until WIFI_TIMEOUT_MS expires.
     */
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, // Do not clear bits on exit
        pdFALSE, // Logical OR condition (unblock if either bit is set)
        pdMS_TO_TICKS(WIFI_TIMEOUT_MS)
    );

    /*
     * STEP 6: Connection Result Evaluation
     */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi Connection established.");
        ret = ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "Failed to connect to SSID: %s after %d tries.", WIFI_SSID, MAXIMUM_RETRY);
        ret = ESP_FAIL;
    } else {
        ESP_LOGW(TAG, "Timeout reached (%d ms) waiting Wi-Fi connection.", WIFI_TIMEOUT_MS);
        ret = ESP_ERR_TIMEOUT;
    }

cleanup:
    /* STEP 7: Cleanup Callbacks and Event Group */
    /* Unregisters temporary listeners and frees Event Group RAM allocation. */
    if (instance_got_ip != NULL) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    }
    if (instance_any_id != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    }
    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    return ret;
}
/**
 * @brief Disconnects and stops the Wi-Fi RF radio to save power.
 */
esp_err_t wifi_disconnect(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Stopping Wi-Fi interface...");
    ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi stopped successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to stop Wi-fi.");
    }

    return ret;
}
