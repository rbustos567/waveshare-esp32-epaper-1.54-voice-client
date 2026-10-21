#include "api_client.h"

static const char *TAG = "API_CLIENT";
static char s_last_cmd_output[1024] = {0};
static char s_remote_cmd_output[256] = {0};

const char* api_client_get_last_cmd_output(void) {
    return s_last_cmd_output;
}

const char* api_client_get_remote_cmd_output(void) {
    return s_remote_cmd_output;
}

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            // Verificamos que la llave no sea nula y coincida sin importar mayúsculas/minúsculas
            if (evt->header_key && strcasecmp(evt->header_key, "X-Cmd-Output") == 0) {
                memset(s_remote_cmd_output, 0, sizeof(s_remote_cmd_output));
                
                if (evt->header_value) {
                    // Copiamos de manera segura usando el tamaño del búfer
                    snprintf(s_remote_cmd_output, sizeof(s_remote_cmd_output), "%s", evt->header_value);
                }
                
                ESP_LOGI("HTTP_EVENT", "¡Header X-Cmd-Output capturado!: %s", s_remote_cmd_output);
            }
            break;
        default:
            break;
    }

    return ESP_OK;
}

esp_err_t api_client_send_audio(char *out_msg, size_t max_len) {
    if (out_msg && max_len > 0) {
        memset(out_msg, 0, max_len);
    }

    ESP_LOGI(TAG, "Conectando al Wi-Fi...");
    if (wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al conectar a internet");
        if (out_msg) snprintf(out_msg, max_len, "WiFi Connection Failed");
        return ESP_FAIL;
    }

    // 1. Obtener datos desde el audio_manager
    uint8_t *audio_data = audio_manager_get_audio_ptr();
    size_t audio_size = audio_manager_get_recorded_bytes();

    if (audio_data == NULL || audio_size == 0) {
        ESP_LOGW(TAG, "No hay datos de audio para enviar.");
        if (out_msg) snprintf(out_msg, max_len, "No audio data recorded");
        wifi_disconnect();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Enviando %d bytes de audio por HTTP...", audio_size);

    // 2. Configurar HTTP
    esp_http_client_config_t config = {};
    config.url = "https://orangepi6plus-1.tail6a8895.ts.net/process-audio";
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 30000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = http_event_handler;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Error al inicializar el cliente HTTP");
        if (out_msg) snprintf(out_msg, max_len, "HTTP Init Error");
        wifi_disconnect();
        return ESP_FAIL;
    }

    // Limpiamos la variable estática al iniciar cada nueva petición HTTP
    memset(s_remote_cmd_output, 0, sizeof(s_remote_cmd_output));

    // Configurar Headers necesarios
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_header(client, "API-Key", "password123x");
    esp_http_client_set_header(client, "X-Communication-Status", "active");

    esp_err_t err = esp_http_client_open(client, audio_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir conexion: %s", esp_err_to_name(err));
        if (out_msg) snprintf(out_msg, max_len, "Connection Error");
        esp_http_client_cleanup(client);
        wifi_disconnect();
        return err;
    }

    // Enviar el buffer de audio por POST
    int bytes_written = esp_http_client_write(client, (const char *)audio_data, audio_size);
    ESP_LOGI("SEND_HTTP", "Enviados %d bytes al servidor", bytes_written);

    // Obtener encabezados de respuesta
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    ESP_LOGI("SEND_HTTP", "Respuesta del servidor. Status = %d, Content-Length = %d", status_code, content_length);

    if (status_code == 200) {
        char *val = NULL;

        // Como el event_handler ya capturó el header al vuelo, 
        // solo evaluamos el contenido de nuestra variable estática:
        if (strlen(s_remote_cmd_output) > 0) {
            ESP_LOGI("SERVER_HEADER", "Cmd Output capturado: %s", s_remote_cmd_output);
            if (out_msg) {
                snprintf(out_msg, max_len, "%s", s_remote_cmd_output);
            }
        } else {
            ESP_LOGW("SERVER_HEADER", "X-Cmd-Output no recibido, usando mensaje por defecto.");
            if (out_msg) {
                snprintf(out_msg, max_len, "Command executed successfully.");
            }
        }

        // Leer metadatos de headers
//        if (esp_http_client_get_header(client, "X-Server-Status", &val) == ESP_OK) {
//            ESP_LOGI("SERVER_HEADER", "Status: %s", val ? val : "(none)");
//        }
//        if (esp_http_client_get_header(client, "X-Transcription", &val) == ESP_OK) {
//            ESP_LOGI("SERVER_HEADER", "Transcription: %s", val ? val : "(none)");
//        }
//        if (esp_http_client_get_header(client, "X-Requires-Followup", &val) == ESP_OK) {
//            ESP_LOGI("SERVER_HEADER", "Requires Followup: %s", val ? val : "(none)");
//        }

//        if (esp_http_client_get_header(client, "X-Cmd-Output", &val) == ESP_OK) {
//            ESP_LOGI("DEBUG", "Header X-Cmd-Output recibido correctamente: '%s'", val ? val : "NULL");
//            if (out_msg && val) {
//                snprintf(out_msg, max_len, "%s", val);
//            }
//        } else {
//            ESP_LOGW("DEBUG", "El header X-Cmd-Output NO fue encontrado o falló.");
//            if (out_msg) {
//                snprintf(out_msg, max_len, "Command executed successfully.");
//            }
//        }


        // Leer el audio binario WAV de la respuesta y reproducirlo en I2S
        uint8_t *pcm_buffer = audio_manager_get_audio_ptr();
        int total_audio_bytes = 0;
        int read_bytes = 0;

        while ((read_bytes = esp_http_client_read(client, (char *)(pcm_buffer + total_audio_bytes), 4096)) > 0) {
            total_audio_bytes += read_bytes;
        }
        ESP_LOGI("SEND_HTTP", "Recibidos %d bytes de audio WAV binario puro", total_audio_bytes);

        if (total_audio_bytes > 44) {
            audio_playback_write(pcm_buffer + 44, total_audio_bytes - 44);
        }
    } else {
        ESP_LOGE("SEND_HTTP", "Error HTTP: %d", status_code);
        if (out_msg) {
            snprintf(out_msg, max_len, "Server Error: HTTP %d", status_code);
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    wifi_disconnect();
    ESP_LOGI(TAG, "Wi-Fi desconectado y radio apagada.");
    return ESP_OK;
}
