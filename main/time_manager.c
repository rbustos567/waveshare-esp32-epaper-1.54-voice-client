#include "time_manager.h"

static const char *TAG = "time_manager";
static int s_current_tz_index = TZ_INDEX_DEFAULT;

static const tz_option_t TZ_OPTIONS[] = {
    {"UTC-8 (PST)", "PST8",        -8}, // O "PST8PDT,M3.2.0,M11.1.0" si quieres DST de EE.UU.
    {"UTC-7 (MST)", "MST7",        -7}, // O "MST7MDT,M3.2.0,M11.1.0"
    {"UTC-6 (CST)", "CST6",        -6}, // ✅ Fijo UTC-6 (Standard Central Time todo el año)
    {"UTC-5 (EST)", "EST5",        -5}, // O "EST5EDT,M3.2.0,M11.1.0"
    {"UTC+0 (GMT)", "GMT0",         0},
    {"UTC+1 (CET)", "CET-1CEST,M3.5.0,M10.5.0", 1}
};

//static const size_t NUM_TZ_OPTIONS = sizeof(TZ_OPTIONS) / sizeof(tz_option_t);

// Static timestamp holding the last synchronized UTC epoch time
static time_t s_last_sync_timestamp = 0;
// Variables guardadas en RTC SLOW RAM
RTC_DATA_ATTR static uint64_t s_rtc_uptime_seconds = 0;
RTC_DATA_ATTR static time_t s_last_sleep_timestamp_sec = 0;

esp_err_t time_manager_init(void) {
    int saved_tz_index = -1;
    nvs_handle_t nvs_h;
   
    ESP_LOGI(TAG, "Entrando en time_manager_init()"); 
    // Intentar abrir la NVS en modo lectura
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_h);
    if (err == ESP_OK) {
        // Leer el índice de la zona horaria guardada
        err = nvs_get_i32(nvs_h, "tz_idx", &saved_tz_index);
        nvs_close(nvs_h);
    }

    if (err == ESP_OK && saved_tz_index >= 0 && saved_tz_index < NUM_TZ_OPTIONS) {
        // Si se encontró en NVS y el valor es válido, actualizamos la RAM local
        s_current_tz_index = saved_tz_index;
        ESP_LOGI(TAG, "Zona horaria recuperada de NVS: %s", TZ_OPTIONS[s_current_tz_index].label);
    } else {
        // Si la clave no existe en NVS (o hubo error), usamos el default
        s_current_tz_index = TZ_INDEX_DEFAULT; // Por ejemplo, el índice de UTC-6
        ESP_LOGI(TAG, "No se encontró zona horaria en NVS, usando valor por defecto: %s", 
                 TZ_OPTIONS[s_current_tz_index].label);
    }

    // Aplicar la zona horaria (RAM -> POSIX setenv / tzset)
    time_manager_set_timezone(TZ_OPTIONS[s_current_tz_index].tz_string);

    return ESP_OK;
}

// Obtener el texto dinámico para renderizar en la lista del menú
void get_timezone_text(char *dest, size_t max_len) {
    snprintf(dest, max_len, "%s", TZ_OPTIONS[s_current_tz_index].label);
}

static void time_sync_notification_cb(struct timeval *tv)
{
    s_last_sync_timestamp = tv->tv_sec;
    ESP_LOGI(TAG, "Notification: Time successfully synchronized via SNTP!");
}

void time_manager_set_timezone(const char *tz_string)
{
    if (tz_string == NULL) {
        return;
    }
    setenv("TZ", tz_string, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to: %s", tz_string);
}

esp_err_t time_manager_sync_sntp(uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Initializing SNTP client...");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_init();

    int retry = 0;
    const int retry_delay_ms = 500;
    const int max_retry = timeout_ms / retry_delay_ms;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < max_retry) {
        ESP_LOGI(TAG, "Waiting for system time sync... (%d/%d)", retry, max_retry);
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
    }

    if (retry >= max_retry) {
        ESP_LOGE(TAG, "SNTP sync timeout reached.");
        esp_sntp_stop();
        return ESP_ERR_TIMEOUT;
    }

    // Save synchronized timestamp to static variable
    time(&s_last_sync_timestamp);

    // Stop SNTP to save power/resources
    esp_sntp_stop();

    return ESP_OK;
}

void time_manager_get_local_time(struct tm *timeinfo)
{
    time_t now;
    time(&now); // Read current Unix timestamp maintained by RTC
    localtime_r(&now, timeinfo);

    ESP_LOGI(TAG, "Epoch: %lld | Hora Local: %02d:%02d | TZ env: %s",
         (long long)now, timeinfo->tm_hour, timeinfo->tm_min, getenv("TZ"));
}

void time_manager_get_formatted_time(char *buffer, size_t max_len, const char *format)
{
    if (buffer == NULL || max_len == 0 || format == NULL) {
        return;
    }

    struct tm timeinfo;
    time_manager_get_local_time(&timeinfo);
    strftime(buffer, max_len, format, &timeinfo);
}

void uptime_manager_init(void)
{
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    // 1. Si es Cold Boot (encendido inicial o botón Reset)
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        s_rtc_uptime_seconds = 0;
        s_last_sleep_timestamp_sec = 0;
        ESP_LOGI(TAG, "Cold Boot detectado: Uptime reiniciado a 0s");
    } else {
        // 2. Viene de Deep Sleep: medir cuánto tiempo estuvo durmiendo con time()
        time_t now_sec = time(NULL);
        if (s_last_sleep_timestamp_sec > 0 && now_sec > s_last_sleep_timestamp_sec) {
            uint64_t slept_seconds = (uint64_t)(now_sec - s_last_sleep_timestamp_sec);
            s_rtc_uptime_seconds += slept_seconds;
            ESP_LOGI(TAG, "Tiempo dormido: %llu s. Uptime acumulado: %llu s",
                     slept_seconds, s_rtc_uptime_seconds);
        }
    }
}

void uptime_prepare_for_sleep(void)
{
    // Sumar el tiempo activo de esta sesión (usando el temporizador local)
    uint64_t active_session_sec = (uint64_t)(esp_timer_get_time() / 1000000LL);
    s_rtc_uptime_seconds += active_session_sec;

    // Guardar la estampa de tiempo actual del RTC antes de apagar la CPU
    s_last_sleep_timestamp_sec = time(NULL);
}

uint32_t uptime_get_seconds(void)
{
    uint64_t current_session_sec = (uint64_t)(esp_timer_get_time() / 1000000LL);
    return (uint32_t)(s_rtc_uptime_seconds + current_session_sec);
}

void uptime_get_formatted(char *buffer, size_t max_len)
{
    if (buffer == NULL || max_len == 0) return;

    uint32_t total_sec = uptime_get_seconds();

    uint32_t days = total_sec / 86400;
    uint32_t hours = (total_sec % 86400) / 3600;
    uint32_t minutes = (total_sec % 3600) / 60;
    uint32_t seconds = total_sec % 60;

    if (days > 0) {
        snprintf(buffer, max_len, "%lud %02luh %02lum %02lus", (unsigned long)days, (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    } else if (hours > 0) {
        snprintf(buffer, max_len, "%02luh %02lum %02lus", (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    } else {
        snprintf(buffer, max_len, "%02lum %02lus", (unsigned long)minutes, (unsigned long)seconds);
    }
}

int get_current_tz_index(void) {
    return s_current_tz_index;
}

void set_current_tz_index(int idx) {
    s_current_tz_index = idx;
}

size_t timezone_get_options_count(void) {
    return NUM_TZ_OPTIONS;
}

const tz_option_t *timezone_get_option_at(size_t index) {
    if (index >= NUM_TZ_OPTIONS) {
        return &TZ_OPTIONS[0]; // Fallback seguro
    }
    return &TZ_OPTIONS[index];
}

const char* get_tz_label_wrapper(int index) {
    const tz_option_t *opt = timezone_get_option_at(index);
    return opt ? opt->label : "";
}
