#include "wifi_manager.h"

static const char *TAG = "WIFI_MANAGER";

// RTC storage block that survives Deep Sleep and feeds both the UI menu and connection logic
RTC_DATA_ATTR struct {
    wifi_credential_t profiles[MAX_WIFI_PROFILES]; // Array of all stored Wi-Fi networks
    int32_t count;                               // Total number of valid profiles loaded
    int32_t active_index;                        // Index of the currently selected network for connection
    bool is_initialized;                         // Flag to check if RTC cache is ready
} rtc_wifi_storage = { .active_index = 0, .is_initialized = false };


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
    char current_ssid[32];
    char current_pass[64];

    // Fetch active credentials instantly from RTC RAM (zero NVS wear)
    ret = wifi_get_active_ssid(current_ssid, sizeof(current_ssid));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve active Wi-Fi SSID!");
        // Handle error accordingly (e.g., return or fallback)
	return ret;
    }

    ret = wifi_get_active_pass(current_pass, sizeof(current_pass));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve active Wi-Fi Password!");
        // Handle error accordingly (e.g., return or fallback)
        return ret;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            },
        },
    };

    // Safely copy the variables into the ESP-IDF Wi-Fi configuration structure
    strlcpy((char *)wifi_config.sta.ssid, current_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, current_pass, sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "Configured Wi-Fi struct with SSID: %s", wifi_config.sta.ssid);
    ESP_LOGI(TAG, "Configured Wi-Fi struct with Pass: %s", wifi_config.sta.password);
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

    ESP_LOGI(TAG, "Starting connection to SSID: %s...", current_ssid);

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
        ESP_LOGW(TAG, "Failed to connect to SSID: %s after %d tries.", current_ssid, MAXIMUM_RETRY);
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

/**
 * @brief Automatically synchronizes Wi-Fi credentials from headers to NVS.
 *        It checks if the network count or any SSID/password has changed.
 *        If a change is detected, it clears old records and updates them.
 */
esp_err_t wifi_sync_credentials_nvs_automatically(void) {
    esp_err_t ret = ESP_OK;
    nvs_handle_t nvs_h;
    
    // Open the NVS "storage" namespace in read/write mode
    if (nvs_open("storage", NVS_READWRITE, &nvs_h) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS 'storage' namespace");
        return ESP_FAIL;
    }

    bool needs_update = false;
    int32_t stored_count = -1;

    // 1. Check if the stored network count matches the current defines count
    if (nvs_get_i32(nvs_h, "wifi_count", &stored_count) != ESP_OK || stored_count != INITIAL_WIFI_COUNT) {
        needs_update = true;
    } else {
        // 2. If count matches, verify profile by profile using a temporary struct array
        struct {
            const char *ssid;
            const char *pass;
        } current_networks[INITIAL_WIFI_COUNT] = {
            {INITIAL_WIFI_SSID_0, INITIAL_WIFI_PASS_0},
            #if INITIAL_WIFI_COUNT > 1
            {INITIAL_WIFI_SSID_1, INITIAL_WIFI_PASS_1}
            #endif
            // Add more profiles here if your defines expand in the future
        };

        for (int i = 0; i < INITIAL_WIFI_COUNT; i++) {
            char k_ssid[16], k_pass[16];
            snprintf(k_ssid, sizeof(k_ssid), "wifi_ssid_%d", i);
            snprintf(k_pass, sizeof(k_pass), "wifi_pass_%d", i);

            char stored_ssid[32] = {0};
            char stored_pass[64] = {0};
            size_t len_ssid = sizeof(stored_ssid);
            size_t len_pass = sizeof(stored_pass);

            // Read currently stored values from NVS
            esp_err_t err_s = nvs_get_str(nvs_h, k_ssid, stored_ssid, &len_ssid);
            esp_err_t err_p = nvs_get_str(nvs_h, k_pass, stored_pass, &len_pass);

            // If reading fails or text doesn't match the defines, an update is required
            if (err_s != ESP_OK || err_p != ESP_OK || 
                strcmp(stored_ssid, current_networks[i].ssid) != 0 || 
                strcmp(stored_pass, current_networks[i].pass) != 0) {
                needs_update = true;
                break; // Exit early as soon as one difference is found
            }
        }
    }

    // 3. If any change was detected, clean up old keys and rewrite with new defines
    if (needs_update) {
        ESP_LOGI(TAG, "Changes detected in Wi-Fi defines! Synchronizing NVS...");

        // Erase old profiles if they previously existed to prevent leftover data
        if (stored_count > 0) {
            for (int i = 0; i < stored_count; i++) {
                char k_ssid[16], k_pass[16];
                snprintf(k_ssid, sizeof(k_ssid), "wifi_ssid_%d", i);
                snprintf(k_pass, sizeof(k_pass), "wifi_pass_%d", i);
                esp_err_t err_es = nvs_erase_key(nvs_h, k_ssid);
		// nvs_erase_key returns esp_err_t, which can be ignored if its NOT_FOUND
		// However, it is useful to validate if occurs a critical hardware/flash error 
		if (err_es != ESP_OK && err_es != ESP_ERR_NVS_NOT_FOUND) {
                    ESP_LOGW(TAG, "Warning: failed to erase %s: %s", k_ssid, esp_err_to_name(err_es));
                }

                esp_err_t err_ep = nvs_erase_key(nvs_h, k_pass);
		if (err_ep != ESP_OK && err_ep != ESP_ERR_NVS_NOT_FOUND) {
                    ESP_LOGW(TAG, "Warning: failed to erase %s: %s", k_pass, esp_err_to_name(err_ep));
                }
            }
        }

        // Save the new network count
        ret = nvs_set_i32(nvs_h, "wifi_count", INITIAL_WIFI_COUNT);
        if (ret != ESP_OK) {
	    ESP_LOGE(TAG, "Failed to set NVS count using key 'wifi_count' in 'storage' namespace");
            nvs_close(nvs_h);
            return ret;
        }   

        // Populate the structured array with current defines
        struct {
            const char *ssid;
            const char *pass;
        } net_list[INITIAL_WIFI_COUNT] = {
            {INITIAL_WIFI_SSID_0, INITIAL_WIFI_PASS_0},
            #if INITIAL_WIFI_COUNT > 1
            {INITIAL_WIFI_SSID_1, INITIAL_WIFI_PASS_1}
            #endif
        };

        // Iteratively save all credentials cleanly using a loop
        for (int i = 0; i < INITIAL_WIFI_COUNT; i++) {
            char k_ssid[16], k_pass[16];
            snprintf(k_ssid, sizeof(k_ssid), "wifi_ssid_%d", i);
            snprintf(k_pass, sizeof(k_pass), "wifi_pass_%d", i);

            ret = nvs_set_str(nvs_h, k_ssid, net_list[i].ssid);
	    if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to write %s to NVS: %s", k_ssid, esp_err_to_name(ret));
                nvs_close(nvs_h);
		return ret;
	    }

            ret = nvs_set_str(nvs_h, k_pass, net_list[i].pass);
	    if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to write %s to NVS: %s", k_pass, esp_err_to_name(ret));
                nvs_close(nvs_h);
                return ret;
            }

        }

        // Commit changes permanently to flash storage
        ret = nvs_commit(nvs_h);
	if (ret != ESP_OK) {
	    ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
	    nvs_close(nvs_h);
	    return ret;
	} else {
            ESP_LOGI(TAG, "NVS successfully synchronized with new Wi-Fi credentials!");
	}
    } else {
        ESP_LOGI(TAG, "No changes in Wi-Fi defines. Using existing NVS data.");
    }

    // Close the NVS handle
    nvs_close(nvs_h);

    return ret;
}

/**
 * @brief Initializes the RTC cache by reading all Wi-Fi profiles from NVS.
 *        To be called once during cold boot / system startup.
 */
esp_err_t wifi_rtc_cache_init(void) {
    esp_err_t ret = ESP_OK;

    nvs_handle_t nvs_h;
    ret = nvs_open("storage", NVS_READONLY, &nvs_h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS 'storage' for reading: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read the total network count stored in NVS
    int32_t stored_count = 0;
    ret = nvs_get_i32(nvs_h, "wifi_count", &stored_count);
    if (ret != ESP_OK || stored_count <= 0) {
        ESP_LOGW(TAG, "No valid Wi-Fi count found in NVS");
        nvs_close(nvs_h);
        return (ret != ESP_OK) ? ret : ESP_ERR_NVS_NOT_FOUND;
    }

    // Prevent overflow if NVS count exceeds our maximum array limit
    if (stored_count > MAX_WIFI_PROFILES) {
        stored_count = MAX_WIFI_PROFILES;
    }

    rtc_wifi_storage.count = stored_count;

    // Iteratively load all SSID and password pairs from NVS into the RTC array
    for (int i = 0; i < stored_count; i++) {
        char k_ssid[16], k_pass[16];
        snprintf(k_ssid, sizeof(k_ssid), "wifi_ssid_%d", i);
        snprintf(k_pass, sizeof(k_pass), "wifi_pass_%d", i);

        size_t len_s = sizeof(rtc_wifi_storage.profiles[i].ssid);
        size_t len_p = sizeof(rtc_wifi_storage.profiles[i].pass);

        esp_err_t err_s = nvs_get_str(nvs_h, k_ssid, rtc_wifi_storage.profiles[i].ssid, &len_s);
	if (err_s != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read %s from NVS: %s", k_ssid, esp_err_to_name(err_s));
            nvs_close(nvs_h);
            return err_s;
        }

        esp_err_t err_p = nvs_get_str(nvs_h, k_pass, rtc_wifi_storage.profiles[i].pass, &len_p);
	if (err_p != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read %s from NVS: %s", k_pass, esp_err_to_name(err_p));
            nvs_close(nvs_h);
            return err_p;
        }
    }

    nvs_close(nvs_h);

    // Ensure active_index is within safe bounds (default to 0 on first boot)
    if (rtc_wifi_storage.active_index < 0 || rtc_wifi_storage.active_index >= rtc_wifi_storage.count) {
        ESP_LOGW(TAG, "Active index (%ld) out of bounds or uninitialized. Resetting to 0.", (long)rtc_wifi_storage.active_index);    
        rtc_wifi_storage.active_index = 0;
    }

    rtc_wifi_storage.is_initialized = true;

    ESP_LOGI(TAG, "RTC cache successfully initialized with %ld profiles. Active index: %ld",
             (long)rtc_wifi_storage.count, (long)rtc_wifi_storage.active_index);

    return ret;
}

esp_err_t wifi_get_active_ssid(char *out_ssid, size_t ssid_len) {
    esp_err_t ret = ESP_OK;

    if (!rtc_wifi_storage.is_initialized) {
        ESP_LOGW(TAG, "RTC cache is not initialized! Call wifi_rtc_cache_init() first.");
        return ESP_ERR_INVALID_STATE;
    }

    int32_t active = rtc_wifi_storage.active_index;
    if (active < 0 || active >= rtc_wifi_storage.count) {
        ESP_LOGE(TAG, "Active Wi-Fi index out of bounds: %ld", (long)active);
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(out_ssid, rtc_wifi_storage.profiles[active].ssid, ssid_len);

    return ret;
}

esp_err_t wifi_get_active_pass(char *out_pass, size_t pass_len) {
    esp_err_t ret = ESP_OK;

    if (!rtc_wifi_storage.is_initialized) {
        ESP_LOGW(TAG, "RTC cache is not initialized! Call wifi_rtc_cache_init() first.");
        return ESP_ERR_INVALID_STATE;
    }

    int32_t active = rtc_wifi_storage.active_index;
    if (active < 0 || active >= rtc_wifi_storage.count) {
        ESP_LOGE(TAG, "Active Wi-Fi index out of bounds: %ld", (long)active);
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(out_pass, rtc_wifi_storage.profiles[active].pass, pass_len);

    return ret;
}

const char* wifi_label_wrapper(int index) {
    
    return wifi_manager_get_ssid_at(index);
}

int wifi_manager_get_active_index(void) {
    
    return (int)rtc_wifi_storage.active_index;
}

int wifi_manager_get_profiles_count(void) {

    return (int)rtc_wifi_storage.count;
}

const char* wifi_manager_get_ssid_at(int index) {
    
    if (index >= 0 && index < rtc_wifi_storage.count) {
        return rtc_wifi_storage.profiles[index].ssid;
    }

    return "";
}

void wifi_manager_set_active_index(int index) {
    
    if (index >= 0 && index < rtc_wifi_storage.count) {
        rtc_wifi_storage.active_index = index;
        ESP_LOGI(TAG, "Active Wi-Fi profile changed to index %d (%s)", index, rtc_wifi_storage.profiles[index].ssid);
    }
}
