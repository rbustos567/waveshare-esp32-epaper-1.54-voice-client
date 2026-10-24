#include "time_manager.h"

static const char *TAG = "TIME_MANAGER";
static int s_current_tz_index = TZ_INDEX_DEFAULT;
static volatile bool s_sntp_synced = false;

static const tz_option_t TZ_OPTIONS[] = {
    {"UTC-8 (PST)", "PST8",        -8}, // O "PST8PDT,M3.2.0,M11.1.0"
    {"UTC-7 (MST)", "MST7",        -7}, // O "MST7MDT,M3.2.0,M11.1.0"
    {"UTC-6 (CST)", "CST6",        -6}, // Fixed UTC-6 (Standard Central Time all year)
    {"UTC-5 (EST)", "EST5",        -5}, // O "EST5EDT,M3.2.0,M11.1.0"
    {"UTC+0 (GMT)", "GMT0",         0},
    {"UTC+1 (CET)", "CET-1CEST,M3.5.0,M10.5.0", 1}
};

// Static timestamp holding the last synchronized UTC epoch time
static time_t s_last_sync_timestamp = 0;
// Variables guardadas en RTC SLOW RAM
RTC_DATA_ATTR static uint64_t s_rtc_uptime_seconds = 0;
RTC_DATA_ATTR static time_t s_last_sleep_timestamp_sec = 0;

esp_err_t time_manager_init(void) {

    esp_err_t ret = ESP_OK;

    int32_t saved_tz_index = -1;
    nvs_handle_t nvs_h;

    ESP_LOGI(TAG, "Entering time_manager_init()");

    // Attempt to open NVS in read-only mode
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_h);
    if (err == ESP_OK) {
        // Read the saved timezone index from NVS
        err = nvs_get_i32(nvs_h, "tz_idx", &saved_tz_index);
        nvs_close(nvs_h);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS storage for reading: %s", esp_err_to_name(err));
    }

    if (err == ESP_OK && saved_tz_index >= 0 && saved_tz_index < (int)NUM_TZ_OPTIONS) {
        // If found in NVS and value is valid, update local RAM state
        s_current_tz_index = saved_tz_index;
        ESP_LOGI(TAG, "Timezone recovered from NVS: %s", TZ_OPTIONS[s_current_tz_index].label);
    } else {
        // If key does not exist or error occurred, use default option
        s_current_tz_index = TZ_INDEX_DEFAULT;
        ESP_LOGI(TAG, "Timezone not found in NVS or invalid, using default: %s", TZ_OPTIONS[s_current_tz_index].label);
    }

    // Apply the timezone (RAM -> POSIX setenv / tzset)
    esp_err_t tz_ret = time_manager_set_timezone(TZ_OPTIONS[s_current_tz_index].tz_string);
    if (tz_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply initial timezone");
        return tz_ret;
    }

    return ret;
}

/**
 * @brief Get dynamic text label for menu list rendering.
 */
esp_err_t get_timezone_text(char *dest, size_t max_len) {

    esp_err_t ret = ESP_OK;

    if (dest == NULL || max_len <= 0) {
        ESP_LOGW(TAG, "Invalid destination buffer provided for timezone text");
        return ESP_ERR_INVALID_ARG;
    }

    int written = snprintf(dest, max_len, "%s", TZ_OPTIONS[s_current_tz_index].label);
    if (written < 0 || (size_t)written >= max_len) {
        ESP_LOGW(TAG, "Timezone text string truncated or formatting error");
        return ESP_ERR_NO_MEM;
    }

    return ret;
}

static void time_sync_notification_cb(struct timeval *tv)
{
    if (tv != NULL) {
        s_last_sync_timestamp = tv->tv_sec;
	s_sntp_synced = true;
	ESP_LOGI(TAG, "Notification: Time successfully synchronized via SNTP!");
    } else {
        ESP_LOGW(TAG, "Notification: SNTP sync callback received a NULL timeval pointer");
    }
}

esp_err_t time_manager_set_timezone(const char *tz_string)
{
    if (tz_string == NULL) {
        ESP_LOGW(TAG, "Provided timezone string is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Set environment variable for POSIX time functions
    if (setenv("TZ", tz_string, 1) != 0) {
        ESP_LOGE(TAG, "Failed to set TZ environment variable");
        return ESP_FAIL;
    }

    // Apply the new timezone rules
    tzset();
    ESP_LOGI(TAG, "Timezone successfully set to: %s", tz_string);

    return ESP_OK;
}

esp_err_t time_manager_sync_sntp(uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    /* Reset flag before starting */
    s_sntp_synced = false;

    ESP_LOGI(TAG, "Initializing SNTP client...");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_init();

    int retry = 0;
    const int retry_delay_ms = 500;
    int max_retry = (int)(timeout_ms / retry_delay_ms);
    if (max_retry <= 0) {
        max_retry = 1;
    }

    /* Wait until time is synchronized via callback flag or timeout is reached */
    while (!s_sntp_synced && ++retry < max_retry) {
        ESP_LOGD(TAG, "Waiting for system time sync... (%d/%d)", retry, max_retry);
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
    }

    /* Check if synchronization failed due to timeout */
    if (!s_sntp_synced) {
        ESP_LOGW(TAG, "SNTP sync timeout reached (%lu ms).", (unsigned long)timeout_ms);
        esp_sntp_stop();
        return ESP_ERR_TIMEOUT;
    }

    /* Save synchronized timestamp to static variable */
    time(&s_last_sync_timestamp);

    /* Stop SNTP to save power/resources */
    esp_sntp_stop();

    ESP_LOGI(TAG, "SNTP synchronization completed successfully.");
    return ret;
}

esp_err_t time_manager_get_local_time(struct tm *timeinfo)
{
    esp_err_t ret = ESP_OK;

    if (timeinfo == NULL) {
        ESP_LOGW(TAG, "Provided timeinfo pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    time_t now;
    time(&now); // Read current Unix timestamp maintained by RTC
    localtime_r(&now, timeinfo);

    ESP_LOGI(TAG, "Epoch: %lld | Hora Local: %02d:%02d | TZ env: %s",
         (long long)now, timeinfo->tm_hour, timeinfo->tm_min, getenv("TZ"));

    return ret;
}

esp_err_t time_manager_get_formatted_time(char *buffer, size_t max_len, const char *format)
{
    esp_err_t ret = ESP_OK;

    if (buffer == NULL || max_len <= 0 || format == NULL) {
        ESP_LOGW(TAG, "Invalid arguments provided for formatted time");
        return ESP_ERR_INVALID_ARG;
    }

    struct tm timeinfo;
    ret = time_manager_get_local_time(&timeinfo);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t written = strftime(buffer, max_len, format, &timeinfo);
    if (written == 0) {
        ESP_LOGW(TAG, "Strftime failed or buffer too small for the given format");
        return ESP_ERR_INVALID_SIZE;
    }

    return ret;
}

esp_err_t uptime_manager_init(void)
{
    esp_err_t ret = ESP_OK;

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    /* STEP 1: Check if it's a Cold Boot (initial power-on or Reset button) */
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        s_rtc_uptime_seconds = 0;
        s_last_sleep_timestamp_sec = 0;
        ESP_LOGI(TAG, "Cold Boot detected: Uptime reset to 0s");
    } else {
        /* STEP 2: Woke up from Deep Sleep: measure time spent sleeping using time() */
        time_t now_sec = time(NULL);
        if (s_last_sleep_timestamp_sec > 0 && now_sec > s_last_sleep_timestamp_sec) {
            uint64_t slept_seconds = (uint64_t)(now_sec - s_last_sleep_timestamp_sec);
            s_rtc_uptime_seconds += slept_seconds;
            ESP_LOGI(TAG, "Time slept: %llu s. Accumulated uptime: %llu s",
                     (unsigned long long)slept_seconds, (unsigned long long)s_rtc_uptime_seconds);
        } else {
            ESP_LOGW(TAG, "Invalid or uninitialized sleep timestamp detected during wake-up evaluation");
            ret = ESP_ERR_INVALID_STATE;
        }
    }

    return ret;
}

void uptime_prepare_for_sleep(void)
{
    // Add the active time of this session (using the local high-resolution timer)
    uint64_t active_session_sec = (uint64_t)(esp_timer_get_time() / 1000000LL);
    s_rtc_uptime_seconds += active_session_sec;

    // Save the current RTC timestamp before shutting down the CPU
    s_last_sleep_timestamp_sec = time(NULL);

    ESP_LOGD(TAG, "Uptime prepared for sleep. Session active: %llu s, Total uptime: %lu s",
             (unsigned long long)active_session_sec, (unsigned long long)s_rtc_uptime_seconds);
}

uint32_t uptime_get_seconds(void)
{
    uint64_t current_session_sec = (uint64_t)(esp_timer_get_time() / 1000000LL);
    
    return (uint32_t)(s_rtc_uptime_seconds + current_session_sec);
}

esp_err_t uptime_get_formatted(char *buffer, size_t max_len)
{
    esp_err_t ret = ESP_OK;

    if (buffer == NULL || max_len == 0) {
        ESP_LOGW(TAG, "Invalid buffer provided for formatted uptime");
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t total_sec = uptime_get_seconds();

    uint32_t days = total_sec / 86400;
    uint32_t hours = (total_sec % 86400) / 3600;
    uint32_t minutes = (total_sec % 3600) / 60;
    uint32_t seconds = total_sec % 60;

    int written = 0;
    if (days > 0) {
        written = snprintf(buffer, max_len, "%lud %02luh %02lum %02lus", (unsigned long)days, (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    } else if (hours > 0) {
       written =  snprintf(buffer, max_len, "%02luh %02lum %02lus", (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    } else {
        written = snprintf(buffer, max_len, "%02lum %02lus", (unsigned long)minutes, (unsigned long)seconds);
    }

    if (written < 0 || (size_t)written >= max_len) {
        ESP_LOGW(TAG, "Formatted uptime string truncated or formatting error");
        return ESP_ERR_NO_MEM;
    }

    return ret;
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
        // Warning: Index out of bounds, falling back to default option (index 0)
        ESP_LOGW(TAG, "Timezone index out of bounds (%zu), max allowed is %d. Falling back to index 0.", index, (int)NUM_TZ_OPTIONS);
        return &TZ_OPTIONS[0];
    }

    return &TZ_OPTIONS[index];
}

const char* get_tz_label_wrapper(int index) {

    // Handle negative index safely before casting to size_t
    if (index < 0) {
        ESP_LOGW(TAG, "Negative timezone index provided (%d). Falling back to index 0.", index);
        index = 0;
    }

    const tz_option_t *opt = timezone_get_option_at((size_t)index);

    return opt ? opt->label : "";
}
