#include "api_client.h"

static const char *TAG = "API_CLIENT";
static char s_last_cmd_output[API_C_CMD_OUT_SIZE] = {0};
static char s_remote_cmd_output[API_C_REMOTE_CMD_OUT_SIZE] = {0};

RTC_DATA_ATTR struct {
    http_server_config_t servers[MAX_HTTP_SERVERS];
    int32_t count;          // Number of servers
    int32_t active_index;   // Index of selected server
    bool is_initialized;    // Flag to know if servers are already cached in RTC
} rtc_server_storage;

const char* api_client_get_last_cmd_output(void) {
    
    return s_last_cmd_output;
}

const char* api_client_get_remote_cmd_output(void) {
    
    return s_remote_cmd_output;
}

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    esp_err_t ret = ESP_OK;

    if (evt == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
	    /* Verify that the header key is not null and matches case-insensitively */
            if (evt->header_key && strcasecmp(evt->header_key, "X-Cmd-Output") == 0) {
                memset(s_remote_cmd_output, 0, sizeof(s_remote_cmd_output));
                
                if (evt->header_value) {
		    /* Safely copy using the buffer size */
                    snprintf(s_remote_cmd_output, sizeof(s_remote_cmd_output), "%s", evt->header_value);
                }
            
	        ESP_LOGI(TAG, "Header X-Cmd-Output captured successfully: %s", s_remote_cmd_output);	
            }
            break;

        default:
            break;
    }

    return ret;
}

esp_err_t api_client_send_audio(char *out_msg, size_t max_len) {
    esp_err_t ret = ESP_OK;

    if (out_msg && max_len > 0) {
        memset(out_msg, 0, max_len);
    }

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    if (wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to internet");
        if (out_msg) 
	    snprintf(out_msg, max_len, "WiFi Connection Failed");

	wifi_disconnect();

        return ESP_FAIL;
    } 

    /* 1. Get data from audio_manager */
    uint8_t *audio_data = audio_manager_get_audio_ptr();
    size_t audio_size = audio_manager_get_recorded_bytes();

    if (audio_data == NULL || audio_size <= 0) {
        ESP_LOGW(TAG, "No audio data to send.");
        if (out_msg) 
	    snprintf(out_msg, max_len, "No audio data to send.");
        wifi_disconnect();

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending %u bytes of audio via HTTP...", (unsigned int)audio_size);

    /* 2. Configure HTTP */
    char server_url[64];

    // Fetch active server url instantly from RTC RAM (zero NVS wear)
    ret = server_get_active_url(server_url, sizeof(server_url));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retrieve active server URL!");
        // Handle error accordingly (e.g., return or fallback)
        return ret;
    }

    esp_http_client_config_t config = {};
    config.url = server_url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = API_C_SERVER_TIMEOUT;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = http_event_handler;

    ESP_LOGI(TAG, "Configured Server struct with URL: %s", config.url);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        if (out_msg) 
	    snprintf(out_msg, max_len, "HTTP Init Error");
        wifi_disconnect();
        
	return ESP_FAIL;
    }

    /* Clean static variable at the start of each new HTTP request */
    memset(s_remote_cmd_output, 0, sizeof(s_remote_cmd_output));

    /* Configure necessary headers */
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_header(client, API_C_SERVER_KEY, API_C_SERVER_PASS);
    esp_http_client_set_header(client, "X-Communication-Status", "active");

    esp_err_t err = esp_http_client_open(client, audio_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        if (out_msg)
	    snprintf(out_msg, max_len, "Connection Error");
        esp_http_client_cleanup(client);
        wifi_disconnect();

        return err;
    }

    /* Send audio buffer via POST */
    int bytes_written = esp_http_client_write(client, (const char *)audio_data, audio_size);
    ESP_LOGI("SEND_HTTP", "Sent %d bytes to server", bytes_written);

    /* Get response headers */
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    ESP_LOGI("SEND_HTTP", "Server response. Status = %d, Content-Length = %d", status_code, content_length);
    
    if (status_code == 200) {

	/* Since event_handler already captured the header on the fly, 
           we only evaluate the contents of our static variable: */
        if (strlen(s_remote_cmd_output) > 0) {
            ESP_LOGI("SERVER_HEADER", "Cmd Output captured: %s", s_remote_cmd_output);
            if (out_msg)
                snprintf(out_msg, max_len, "%s", s_remote_cmd_output);
            
        } else {
            ESP_LOGW("SERVER_HEADER", "X-Cmd-Output not received, using default message.");
            if (out_msg)
                snprintf(out_msg, max_len, "Command executed successfully.");
            
        }

        /* Read raw WAV audio binary response and play it */
        uint8_t *pcm_buffer = audio_manager_get_audio_ptr();
        int total_audio_bytes = 0;
        int read_bytes = 0;

        while ((read_bytes = esp_http_client_read(client, (char *)(pcm_buffer + total_audio_bytes), 4096)) > 0) {
            total_audio_bytes += read_bytes;
        }
        ESP_LOGI("SEND_HTTP", "Received %d bytes of pure raw WAV audio", total_audio_bytes);

        if (total_audio_bytes > 44)
            audio_playback_write(pcm_buffer + 44, total_audio_bytes - 44);
    } else {
        ESP_LOGE("SEND_HTTP", "HTTP Error: %d", status_code);
        if (out_msg)
            snprintf(out_msg, max_len, "Server Error: HTTP %d", status_code);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    wifi_disconnect();
    ESP_LOGI(TAG, "Wi-Fi disconnected and radio powered down.");

    return ESP_OK;
}

/**
 * @brief Automatically synchronizes HTTP server profiles from headers to NVS.
 *        It checks if the server count or any URL has changed.
 *        If a change is detected, it clears old records and updates them.
 */
esp_err_t server_sync_config_nvs_automatically(void) {
    esp_err_t ret = ESP_OK;
    nvs_handle_t nvs_h;

    // Open the NVS "storage" namespace in read/write mode
    if (nvs_open("storage", NVS_READWRITE, &nvs_h) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS 'storage' namespace");
        return ESP_FAIL;
    }

    bool needs_update = false;
    int32_t stored_count = -1;

    // 1. Check if the stored server count matches the current defines count
    if (nvs_get_i32(nvs_h, "server_count", &stored_count) != ESP_OK || stored_count != INITIAL_SERVER_COUNT) {
        needs_update = true;
    } else {
        // 2. If count matches, verify profile by profile using a temporary struct array
        struct {
            const char *url;
        } current_servers[INITIAL_SERVER_COUNT] = {
            {INITIAL_SERVER_URL_0},
            #if INITIAL_SERVER_COUNT > 1
            {INITIAL_SERVER_URL_1},
            #endif
            // Add more profiles here if your defines expand in the future
        };

        for (int i = 0; i < INITIAL_SERVER_COUNT; i++) {
            char k_url[16];
            snprintf(k_url, sizeof(k_url), "server_url_%d", i);

            char stored_url[64] = {0};
            size_t len_url = sizeof(stored_url);

            // Read currently stored values from NVS
            esp_err_t err_u = nvs_get_str(nvs_h, k_url, stored_url, &len_url);

            // If reading fails or text doesn't match the defines, an update is required
            if (err_u != ESP_OK || strcmp(stored_url, current_servers[i].url) != 0) {
                needs_update = true;
                break; // Exit early as soon as one difference is found
            }
        }
    }

    // 3. If any change was detected, clean up old keys and rewrite with new defines
    if (needs_update) {
        ESP_LOGI(TAG, "Changes detected in Server defines! Synchronizing NVS...");

        // Erase old profiles if they previously existed to prevent leftover data
        if (stored_count > 0) {
            for (int i = 0; i < stored_count; i++) {
                char k_url[16];
                snprintf(k_url, sizeof(k_url), "server_url_%d", i);
                esp_err_t err_es = nvs_erase_key(nvs_h, k_url);
                if (err_es != ESP_OK && err_es != ESP_ERR_NVS_NOT_FOUND) {
                    ESP_LOGW(TAG, "Warning: failed to erase %s: %s", k_url, esp_err_to_name(err_es));
                }
            }
        }

        // Save the new server count
        ret = nvs_set_i32(nvs_h, "server_count", INITIAL_SERVER_COUNT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set NVS count using key 'server_count' in 'storage' namespace");
            nvs_close(nvs_h);
            return ret;
        }

        // Populate the structured array with current defines
        struct {
            const char *url;
        } server_list[INITIAL_SERVER_COUNT] = {
            {INITIAL_SERVER_URL_0},
            #if INITIAL_SERVER_COUNT > 1
            {INITIAL_SERVER_URL_1},
            #endif
        };

        // Iteratively save all URLs cleanly using a loop
        for (int i = 0; i < INITIAL_SERVER_COUNT; i++) {
            char k_url[16];
            snprintf(k_url, sizeof(k_url), "server_url_%d", i);

            ret = nvs_set_str(nvs_h, k_url, server_list[i].url);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write %s to NVS: %s", k_url, esp_err_to_name(ret));
                nvs_close(nvs_h);
                return ret;
            }
        }

        // Commit changes permanently to flash storage
        ret = nvs_commit(nvs_h);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "NVS successfully synchronized with new Server configurations!");
        }
    } else {
        ESP_LOGI(TAG, "No changes in Server defines. Using existing NVS data.");
    }

    nvs_close(nvs_h);
    return ret;
}

/**
 * @brief Caches HTTP server profiles from NVS into RTC RAM during cold boot.
 *        To be called once during cold boot / system startup.
 */
esp_err_t server_rtc_cache_init(void) {
    esp_err_t ret = ESP_OK;
    nvs_handle_t nvs_h;

    ret = nvs_open("storage", NVS_READONLY, &nvs_h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS 'storage' for reading: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read the total server count stored in NVS
    int32_t stored_count = 0;
    ret = nvs_get_i32(nvs_h, "server_count", &stored_count);
    if (ret != ESP_OK || stored_count <= 0) {
        ESP_LOGW(TAG, "No valid Server count found in NVS");
        nvs_close(nvs_h);
        return (ret != ESP_OK) ? ret : ESP_ERR_NVS_NOT_FOUND;
    }

    // Prevent overflow if NVS count exceeds our maximum array limit
    if (stored_count > MAX_HTTP_SERVERS) {
        stored_count = MAX_HTTP_SERVERS;
    }

    rtc_server_storage.count = stored_count;

    // Iteratively load all server URLs from NVS into the RTC array
    for (int i = 0; i < stored_count; i++) {
        char k_url[16];
        snprintf(k_url, sizeof(k_url), "server_url_%d", i);

        size_t len_u = sizeof(rtc_server_storage.servers[i].url);

        esp_err_t err_u = nvs_get_str(nvs_h, k_url, rtc_server_storage.servers[i].url, &len_u);
        if (err_u != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read %s from NVS: %s", k_url, esp_err_to_name(err_u));
            nvs_close(nvs_h);
            return err_u;
        }
    }

    nvs_close(nvs_h);

    // Ensure active_index is within safe bounds (default to 0 on first boot)
    if (rtc_server_storage.active_index < 0 || rtc_server_storage.active_index >= rtc_server_storage.count) {
        ESP_LOGW(TAG, "Active server index (%ld) out of bounds or uninitialized. Resetting to 0.", (long)rtc_server_storage.active_index);
        rtc_server_storage.active_index = 0;
    }

    rtc_server_storage.is_initialized = true;

    ESP_LOGI(TAG, "RTC server cache successfully initialized with %ld profiles. Active index: %ld",
             (long)rtc_server_storage.count, (long)rtc_server_storage.active_index);

    return ret;
}

esp_err_t server_get_active_url(char *out_url, size_t max_len) {
    esp_err_t ret = ESP_OK;

    if (!rtc_server_storage.is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    int idx = rtc_server_storage.active_index;
    if (idx < 0 || idx >= rtc_server_storage.count) {
        idx = 0; // Fallback
    }

    strlcpy(out_url, rtc_server_storage.servers[idx].url, max_len);
    
    return ret;
}

const char* server_label_wrapper(int index) {

    return server_get_url_at(index);
}

int server_get_active_index(void) {

    return (int)rtc_server_storage.active_index;
}

int server_get_profiles_count(void) {

    return (int)rtc_server_storage.count;
}

const char* server_get_url_at(int index) {

    if (index >= 0 && index < rtc_server_storage.count) {
        return rtc_server_storage.servers[index].url;
    }

    return "";
}

esp_err_t server_set_active_index(int index) {
    esp_err_t ret = ESP_OK;

    if (index < 0 || index >= rtc_server_storage.count) {
        ESP_LOGE(TAG, "Invalid server index: %d", index);
        return ESP_ERR_INVALID_ARG;
    }

    rtc_server_storage.active_index = index;

    ESP_LOGI(TAG, "Active server profile changed to index %d (%s)", index, rtc_server_storage.servers[index].url);

    return ret;
}
