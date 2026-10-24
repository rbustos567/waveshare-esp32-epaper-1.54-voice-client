#include "api_client.h"

static const char *TAG = "API_CLIENT";
static char s_last_cmd_output[API_C_CMD_OUT_SIZE] = {0};
static char s_remote_cmd_output[API_C_REMOTE_CMD_OUT_SIZE] = {0};

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
    esp_http_client_config_t config = {};
    config.url = API_C_SERVER_URL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = API_C_SERVER_TIMEOUT;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = http_event_handler;

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
