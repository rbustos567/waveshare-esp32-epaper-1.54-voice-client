#include "power_manager.h"

static const char *TAG = "POWER_MANAGER";
static TimerHandle_t s_inactivity_timer = NULL;
static SemaphoreHandle_t s_sleep_semaphore = NULL;

static const sleep_timeout_option_t TIMEOUT_OPTIONS[] = {
    {"30 sec", 30},
    {"1 min",  60},
    {"2 min", 120},
    {"5 min", 300}
};

static const size_t NUM_TIMEOUT_OPTIONS = sizeof(TIMEOUT_OPTIONS) / sizeof(sleep_timeout_option_t);
static int s_current_timeout_index = SLEEP_TIMEOUT_INDEX_DEFAULT; // Default: 2 min (index 2)

/**
 * @brief Prepares GPIO0 (BOOT button) and puts the ESP32 into Deep Sleep.
 */
esp_err_t enter_deep_sleep_boot_button(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Configuring BOOT button (GPIO0) as wake-up source...");

    /*
     * STEP 1: Enable EXT0 Wakeup on GPIO0
     * Trigger wakeup when GPIO0 goes to 0 (button pressed to GND).
     */
    ret = esp_sleep_enable_ext0_wakeup(BOOT_BUTTON_GPIO, BOOT_BUTTON_LEVEL);
    if (ret != ESP_OK) {
        return ret;
    }

    /*
     * STEP 2: Configure RTC Pull-Up
     * Ensures GPIO0 stays HIGH (1) while sleeping and doesn't trigger false wakeups.
     */
    ret = rtc_gpio_pullup_en(BOOT_BUTTON_GPIO);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = rtc_gpio_pulldown_dis(BOOT_BUTTON_GPIO);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Entering Deep Sleep. Press BOOT button to wake up.");
    fflush(stdout);

    /* STEP 3: Start Deep Sleep */
    // Keep the power hold pin state HIGH during deep sleep
    ret = rtc_gpio_hold_en(GPIO_NUM_17);
    if (ret != ESP_OK) {
        return ret;
    }

    /*
     * Start Deep Sleep
     */
    esp_deep_sleep_start();

    return ret;
}

/**
 * @brief Callback of timer (Executes in Tmr Svc).
 * MUST BE VERY LIGHT to avoid overflow of stack
 */
static void inactivity_timeout_cb(TimerHandle_t xTimer)
{
    if (s_sleep_semaphore != NULL) {
	// Gives the semaphore to unblock the dedicated task
        xSemaphoreGive(s_sleep_semaphore);
    }
}

/**
 * @brief Dedicated Task to manage entrance of deep sleep 
 */
static void power_task(void *pvParameters)
{
    while (1) {
	// Remains blocked here without consuming CPU until callback of semaphore
        if (xSemaphoreTake(s_sleep_semaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Inactivity detected. Entering Deep Sleep...");
           
	    set_current_ui_state(UI_STATE_SPLASH);
            render_full("SLEEPING", NULL); 
            uptime_prepare_for_sleep();
            enter_deep_sleep_boot_button();
        }
    }
}

esp_err_t power_manager_init(void) {

    esp_err_t ret = ESP_OK;
    // Tries to obtain index from NVS
    int32_t saved_index = 0;
    nvs_handle_t nvs_h;

    if (nvs_open("storage", NVS_READONLY, &nvs_h) == ESP_OK) {
        if (nvs_get_i32(nvs_h, "sleep_tout_idx", &saved_index) == ESP_OK) {
            if (saved_index >= 0 && saved_index < (int32_t)NUM_TIMEOUT_OPTIONS) {
                s_current_timeout_index = saved_index;
            }
        }
        nvs_close(nvs_h);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS storage, using default timeout");
    }

    if (s_current_timeout_index < 0 || s_current_timeout_index >= (int)NUM_TIMEOUT_OPTIONS) {
        s_current_timeout_index = SLEEP_TIMEOUT_INDEX_DEFAULT; // Fallback al default de 30s
	ESP_LOGW(TAG, "Invalid timeout index detected, falling back to default: %d", SLEEP_TIMEOUT_INDEX_DEFAULT);
    }

    // Get seconds according to the recovered index (or default 0 -> 30s)
    uint32_t timeout_seconds = TIMEOUT_OPTIONS[s_current_timeout_index].value_seconds;

    // STEP 1: Create the binary semaphore for synchronization
    if (s_sleep_semaphore == NULL) {
        
	s_sleep_semaphore = xSemaphoreCreateBinary();
	if (s_sleep_semaphore == NULL) {
            ESP_LOGE(TAG, "Failed to create sleep binary semaphore");
            return ESP_ERR_NO_MEM;
        }
    }

    // STEP 2: Create the dedicated power task
    BaseType_t task_ret = xTaskCreate(
        power_task,
        "power_task",
        3072,
        NULL,
        5,
        NULL
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create power_task");
        return ESP_ERR_NO_MEM;
    }

    // STEP 3: Create and start the inactivity timer with the active timeout
    s_inactivity_timer = xTimerCreate(
        "InactivityTimer",
        pdMS_TO_TICKS(timeout_seconds * 1000),
        pdFALSE, // One-shot
        (void *)0,
        inactivity_timeout_cb
    );

    if (s_inactivity_timer != NULL) {
        if (xTimerStart(s_inactivity_timer, 0) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start inactivity timer");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Timer de inactividad iniciado con %s (%d s)", 
                 TIMEOUT_OPTIONS[s_current_timeout_index].label, timeout_seconds);
    } else {
        ESP_LOGE(TAG, "Failed to create inactivity timer");
        return ESP_ERR_NO_MEM;
    }

    return ret;
}

esp_err_t power_manager_reset_inactivity(void)
{
    esp_err_t ret = ESP_OK;

    if (s_inactivity_timer != NULL) {
	// Reload the timer to its initial value (restarts the countdown)
	if (xTimerReset(s_inactivity_timer, 0) == pdPASS) {
            ESP_LOGD(TAG, "Inactivity timer reset successfully");
        } else {
            ESP_LOGW(TAG, "Failed to send reset command to inactivity timer");
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGW(TAG, "Inactivity timer is NULL, cannot reset");
        ret = ESP_ERR_INVALID_STATE;
    }

    return ret;
}

size_t power_manager_get_timeout_options_count(void) {
    
    return NUM_TIMEOUT_OPTIONS;
}

const sleep_timeout_option_t* power_manager_get_timeout_option_at(size_t index) {

    if (index >= NUM_TIMEOUT_OPTIONS) {
	// Warning: Index out of bounds, falling back to default option (index 0)
        ESP_LOGW(TAG, "Index out of bounds (%zu), max allowed is %d. Falling back to index 0.", index, (int)NUM_TIMEOUT_OPTIONS);
        return &TIMEOUT_OPTIONS[0]; 
    }

    return &TIMEOUT_OPTIONS[index];
}

int power_manager_get_current_timeout_index(void) {
    
    return s_current_timeout_index;
}

esp_err_t power_manager_set_timeout_index(int index) {

    esp_err_t ret = ESP_OK;

    if (index < 0 || index >= (int)NUM_TIMEOUT_OPTIONS) {
        ESP_LOGW(TAG, "Invalid timeout index provided: %d", index);
        return ESP_ERR_INVALID_ARG;
    }

    s_current_timeout_index = index;
    int seconds = TIMEOUT_OPTIONS[s_current_timeout_index].value_seconds;

    // STEP 1: Reconfigure active inactivity timer
    if (s_inactivity_timer != NULL) {
        TickType_t new_period_ticks = pdMS_TO_TICKS(seconds * 1000);
        // xTimerChangePeriod reconfigures the period and automatically resets the count
        if (xTimerChangePeriod(s_inactivity_timer, new_period_ticks, 0) == pdPASS) {
            ESP_LOGI(TAG, "Inactivity timer reconfigured to %d s", seconds);
        } else {
            ESP_LOGW(TAG, "Failed to reconfigure inactivity timer period");
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGW(TAG, "Inactivity timer is NULL, cannot reconfigure period");
        ret = ESP_ERR_INVALID_STATE;
    }
    
    // STEP 2: Persist to NVS
    nvs_handle_t nvs_h;
    esp_err_t nvs_err = nvs_open("storage", NVS_READWRITE, &nvs_h);
    if (nvs_err == ESP_OK) {
        nvs_err = nvs_set_i32(nvs_h, "sleep_tout_idx", s_current_timeout_index);
        if (nvs_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set timeout index in NVS: %s", esp_err_to_name(nvs_err));
            ret = nvs_err;
        } else {
            nvs_err = nvs_commit(nvs_h);
            if (nvs_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(nvs_err));
                ret = nvs_err;
            } else {
                ESP_LOGI(TAG, "Sleep timeout saved to NVS: %s (%d s)", 
                         TIMEOUT_OPTIONS[s_current_timeout_index].label, seconds);
            }
        }
        nvs_close(nvs_h);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS storage for writing: %s", esp_err_to_name(nvs_err));
        ret = nvs_err;
    }

    return ret;
}

const char* get_timeout_label_wrapper(int index) {

    const sleep_timeout_option_t *opt = power_manager_get_timeout_option_at(index);
    
    return opt ? opt->label : "";
}
