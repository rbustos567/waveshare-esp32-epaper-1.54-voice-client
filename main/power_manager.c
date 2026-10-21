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
static int s_current_timeout_index = SLEEP_TIMEOUT_INDEX_DEFAULT; // Default: 2 min (índice 2)

/**
 * @brief Prepares GPIO0 (BOOT button) and puts the ESP32 into Deep Sleep.
 */
void enter_deep_sleep_boot_button(void)
{
    ESP_LOGI(TAG, "Configuring BOOT button (GPIO0) as wake-up source...");

    /*
     * STEP 1: Enable EXT0 Wakeup on GPIO0
     * Trigger wakeup when GPIO0 goes to 0 (button pressed to GND).
     */
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(BOOT_BUTTON_GPIO, BOOT_BUTTON_LEVEL));

    /*
     * STEP 2: Configure RTC Pull-Up
     * Ensures GPIO0 stays HIGH (1) while sleeping and doesn't trigger false wakeups.
     */
    rtc_gpio_pullup_en(BOOT_BUTTON_GPIO);
    rtc_gpio_pulldown_dis(BOOT_BUTTON_GPIO);

    ESP_LOGI(TAG, "Entering Deep Sleep. Press BOOT button to wake up.");
    fflush(stdout);

    /*
     * STEP 3: Start Deep Sleep
     */
    esp_deep_sleep_start();
}

/**
 * @brief Callback del timer (se ejecuta en Tmr Svc).
 * DEBE SER ULTRA LIGERO para evitar desbordamiento de pila.
 */
static void inactivity_timeout_cb(TimerHandle_t xTimer)
{
    if (s_sleep_semaphore != NULL) {
        // Da el semáforo para desbloquear la tarea dedicada (no hace logs ni operaciones pesadas)
        xSemaphoreGive(s_sleep_semaphore);
    }
}

/**
 * @brief Tarea dedicada para gestionar la entrada a Deep Sleep.
 */
static void power_task(void *pvParameters)
{
    while (1) {
        // Se queda bloqueada aquí sin consumir CPU hasta que el callback dé el semáforo
        if (xSemaphoreTake(s_sleep_semaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Inactividad detectada. Entrando a Deep Sleep...");
           
	    // Dejamos pantall de Splash con status SLEEP
	    set_current_ui_state(UI_STATE_SPLASH);
	    //render_splash_to_buffer("SLEEPING");
            render_full("SLEEPING", NULL); 
	    // Preparar el estado y entrar a Deep Sleep de forma segura
            uptime_prepare_for_sleep();
            enter_deep_sleep_boot_button();
        }
    }
}

void power_manager_init(void) {
    // A. Intentar restaurar el índice desde NVS
    int32_t saved_index = 0;
    nvs_handle_t nvs_h;
    if (nvs_open("storage", NVS_READONLY, &nvs_h) == ESP_OK) {
        if (nvs_get_i32(nvs_h, "sleep_tout_idx", &saved_index) == ESP_OK) {
            if (saved_index >= 0 && saved_index < (int32_t)NUM_TIMEOUT_OPTIONS) {
                s_current_timeout_index = saved_index;
            }
        }
        nvs_close(nvs_h);
    }

    if (s_current_timeout_index < 0 || s_current_timeout_index >= (int)NUM_TIMEOUT_OPTIONS) {
        s_current_timeout_index = SLEEP_TIMEOUT_INDEX_DEFAULT; // Fallback al default de 30s
    }

    // Obtener los segundos según el índice recuperado (o el default 0 -> 30s)
    uint32_t timeout_seconds = TIMEOUT_OPTIONS[s_current_timeout_index].value_seconds;

    // 1. Crear el semáforo binario para sincronización
    if (s_sleep_semaphore == NULL) {
        s_sleep_semaphore = xSemaphoreCreateBinary();
    }

    // 2. Crear la tarea dedicada de energía
    xTaskCreate(
        power_task,
        "power_task",
        3072,
        NULL,
        5,
        NULL
    );

    // 3. Crear e iniciar el temporizador de inactividad con el timeout activo
    s_inactivity_timer = xTimerCreate(
        "InactivityTimer",
        pdMS_TO_TICKS(timeout_seconds * 1000),
        pdFALSE, // One-shot
        (void *)0,
        inactivity_timeout_cb
    );

    if (s_inactivity_timer != NULL) {
        xTimerStart(s_inactivity_timer, 0);
        ESP_LOGI(TAG, "Timer de inactividad iniciado con %s (%d s)", 
                 TIMEOUT_OPTIONS[s_current_timeout_index].label, timeout_seconds);
    }
}

void power_manager_reset_inactivity(void)
{
    if (s_inactivity_timer != NULL) {
        // Recarga el temporizador al valor inicial (reinicia la cuenta a 0)
	ESP_LOGI(TAG, "Reseteando temporaizador...");
        xTimerReset(s_inactivity_timer, 0);
    }
}

size_t power_manager_get_timeout_options_count(void) {
    return NUM_TIMEOUT_OPTIONS;
}

const sleep_timeout_option_t* power_manager_get_timeout_option_at(size_t index) {
    if (index >= NUM_TIMEOUT_OPTIONS) {
        return &TIMEOUT_OPTIONS[0]; // Fallback seguro
    }

    return &TIMEOUT_OPTIONS[index];
}

int power_manager_get_current_timeout_index(void) {
    return s_current_timeout_index;
}

void power_manager_set_timeout_index(int index) {
    if (index < 0 || index >= (int)NUM_TIMEOUT_OPTIONS) {
        return;
    }

    s_current_timeout_index = index;
    int seconds = TIMEOUT_OPTIONS[s_current_timeout_index].value_seconds;

    // 1. Reconfigurar el timer activo de inactividad
    if (s_inactivity_timer != NULL) {
        TickType_t new_period_ticks = pdMS_TO_TICKS(seconds * 1000);
        // xTimerChangePeriod reconfigura el tiempo y reinicia la cuenta automáticamente
        if (xTimerChangePeriod(s_inactivity_timer, new_period_ticks, 0) == pdPASS) {
            ESP_LOGI(TAG, "Timer de inactividad reconfigurado a %d s", seconds);
        }
    }

    // 2. Persistir en NVS
    nvs_handle_t nvs_h;
    if (nvs_open("storage", NVS_READWRITE, &nvs_h) == ESP_OK) {
        nvs_set_i32(nvs_h, "sleep_tout_idx", s_current_timeout_index);
        nvs_commit(nvs_h);
        nvs_close(nvs_h);
        ESP_LOGI(TAG, "Sleep timeout guardado en NVS: %s (%d s)", 
                 TIMEOUT_OPTIONS[s_current_timeout_index].label, seconds);
    }
}

const char* get_timeout_label_wrapper(int index) {
    const sleep_timeout_option_t *opt = power_manager_get_timeout_option_at(index);
    return opt ? opt->label : "";
}
