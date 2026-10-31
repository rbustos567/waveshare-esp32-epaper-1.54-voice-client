#include "callbacks.h"

static const char *TAG = "CALLBACKS";

/**
 * @brief Cancels the active voice session and stops physical recording if active.
 */
void action_cancel_voice_session(void)
{
    set_cancel_voice_session(true);

    if (audio_manager_is_recording()) {
        /* Force stop physical recording if it was active */
        audio_manager_toggle_recording();
    }
}

/**
 * @brief Background orchestration task for recording audio and sending it via HTTP.
 */
static void voice_session_task(void *pvParameters) {

    ESP_LOGI(TAG, "Starting voice session task...");

    set_cancel_voice_session(false);

    /* 1. STATE: Recording audio */
    /* Update visual screen */
    render_partial_with_text("Recording...", "Speak your command...");

    /* Start recording (first double-click trigger) */
    audio_manager_toggle_recording();

    /* Wait while the user is recording (until the second double-click or cancellation) */
    while (audio_manager_is_recording() && !get_cancel_voice_session()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* IF THE USER CANCELLED WITH A SINGLE CLICK: */
    if (get_cancel_voice_session()) {
        ESP_LOGI(TAG, "Voice session cancelled by user. Discarding audio.");

        /* Ensure recording stops so record_task exits its loop and terminates */
        if (audio_manager_is_recording()) {
            audio_manager_toggle_recording();
        }

        vTaskDelete(NULL);
        return;
    }

    /* 2. STATE: Sending HTTP request */
    if (audio_manager_get_recorded_bytes() > 0) {
        
        /* ENABLE TOTAL LOCK: Nobody can interrupt HTTP communication */
        set_is_waiting_response(true);

        ESP_LOGI(TAG, "Sending audio via HTTP...");
        render_partial_with_text("Sending to server...", "Uploading audio to server...");

        char server_response_buf[API_C_REMOTE_CMD_OUT_SIZE] = {0};

        /* Execute HTTP request (blocks briefly until response is received) */
        esp_err_t err = api_client_send_audio(server_response_buf, sizeof(server_response_buf));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send audio via HTTP: %s", esp_err_to_name(err));
            snprintf(server_response_buf, sizeof(server_response_buf), "Network Error");
        }

        const char *server_response = api_client_get_remote_cmd_output();

        if (server_response != NULL && strlen(server_response) > 0) {
            render_partial_with_text("Ready to record", server_response);
        } else {
            render_partial_with_text("Ready to record", "Done.");
        }

        /* RELEASE TOTAL LOCK: The cycle has finished successfully */
        set_is_waiting_response(false);

    } else {
        ESP_LOGW(TAG, "No audio recorded.");
        set_is_waiting_response(false);
    }

    vTaskDelete(NULL);
}

/**
 * @brief Triggers the voice recording and sending workflow via FreeRTOS task.
 */
void action_record_and_send_audio(void)
{
    if (!audio_manager_is_recording() && !get_is_waiting_response()) {
        /* Change UI state to block menu interactions */
        set_current_ui_state(UI_STATE_VOICE_RECORDING);

        /* Launch orchestration task on Core 1 */
        BaseType_t task_created = xTaskCreatePinnedToCore(
            voice_session_task,
            "voice_session_task",
            4096,
            NULL,
            5,
            NULL,
            1
        );

        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create voice_session_task");
            // Fallback: restore UI state if task creation failed
            set_current_ui_state(UI_STATE_MENU);
	    reset_to_root_menu();
            render_full(NULL, NULL);

            return;
        }
    } else if (audio_manager_is_recording()) {
        /* Second double click: stops ongoing recording */
        esp_err_t err = audio_manager_toggle_recording();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to toggle audio recording stop: %s", esp_err_to_name(err));
        }
    }
}
/**
 * @brief Toggles audio recording state.
 */
void action_record_audio(void) {

    audio_manager_toggle_recording();
}

/**
 * @brief Saves the selected timezone index to NVS and applies it immediately.
 */
void action_save_timezone(int selected_index)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Before set_current_tz_index");
    set_current_tz_index(selected_index);

    /* 1. Save to NVS storage */
    nvs_handle_t nvs_h;
    ret = nvs_open("storage", NVS_READWRITE, &nvs_h);
    if (ret == ESP_OK) {
        ret = nvs_set_i32(nvs_h, "tz_idx", (int32_t)selected_index);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write tz_idx to NVS: %s", esp_err_to_name(ret));
        } else {
            ret = nvs_commit(nvs_h);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
            }
        }
        nvs_close(nvs_h);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS namespace 'storage': %s", esp_err_to_name(ret));
    }

    /* 2. Apply immediately to C library / ESP-IDF system */
    const tz_option_t *tz_option = timezone_get_option_at(selected_index);
    if (tz_option != NULL && tz_option->tz_string != NULL) {
        setenv("TZ", tz_option->tz_string, 1);
        tzset();
        ESP_LOGI(TAG, "New TimeZone set: %s", tz_option->tz_string);
    } else {
        ESP_LOGW(TAG, "Invalid timezone option or empty tz_string");
    }

    ESP_LOGI(TAG, "Timezone save completed.");
}

/**
 * @brief Opens the timezone configuration editor UI.
 */
void action_open_tz_editor(void)
{
    set_menu_active_editor_title("Set TimeZone");

    int tz_index = get_current_tz_index();
    set_menu_active_editor_current_val(tz_index);
    set_menu_active_editor_min_val(0);

    int max_val = timezone_get_options_count() - 1;
    set_menu_active_editor_max_val(max_val);

    set_menu_active_editor_step(1);
    set_menu_active_editor_unit_suffix("");
    set_menu_active_editor_get_label(get_tz_label_wrapper);
    set_menu_active_editor_on_save(action_save_timezone);
    set_menu_active_editor_focus_index(EDITOR_FOCUS_PLUS);
    set_current_ui_state(UI_STATE_EDIT_VALUE);
    render_full(NULL, NULL);
}

/**
 * @brief Saves the selected sleep timeout index.
 */
void action_save_sleep_timeout(int selected_index)
{
    power_manager_set_timeout_index(selected_index);
    ESP_LOGI(TAG, "Sleep timeout index saved: %d", selected_index);
}

/**
 * @brief Saves the selected wifi ssid index.
 */
void action_save_wifi_selection(int index) {
    wifi_manager_set_active_index(index);
    ESP_LOGI(TAG, "Wifi SSID index saved: %d", index);
}

/**
 * @brief Saves the selected server index.
 */
void action_save_server_selection(int index) {
    server_set_active_index(index);
    ESP_LOGI(TAG, "Server index saved: %d", index);
}

/**
 * @brief Opens the sleep timeout configuration editor UI.
 */
void action_open_sleep_timeout_editor(void)
{
    set_menu_active_editor_title("Set Sleep Timeout");

    int timeout_index = power_manager_get_current_timeout_index();
    set_menu_active_editor_current_val(timeout_index);
    set_menu_active_editor_min_val(0);

    int max_val = (int)power_manager_get_timeout_options_count() - 1;
    set_menu_active_editor_max_val(max_val);

    set_menu_active_editor_step(1);
    set_menu_active_editor_unit_suffix(""); /* Optional, labels already include units */
    set_menu_active_editor_get_label(get_timeout_label_wrapper);
    set_menu_active_editor_on_save(action_save_sleep_timeout);
    set_menu_active_editor_focus_index(EDITOR_FOCUS_PLUS);
    set_current_ui_state(UI_STATE_EDIT_VALUE);
    render_full(NULL, NULL);
}

/**
 * @brief Opens the Choose Network configuration editor UI. 
 */
void action_open_choose_network_editor(void)
{
    set_menu_active_editor_title("Choose Network");

    int current_index = wifi_manager_get_active_index();
    set_menu_active_editor_current_val(current_index);
    set_menu_active_editor_min_val(0);

    int max_val = wifi_manager_get_profiles_count() - 1;
    if (max_val < 0) 
        max_val = 0; // Fallback
    set_menu_active_editor_max_val(max_val);

    set_menu_active_editor_step(1);
    set_menu_active_editor_unit_suffix(""); /* Optional, labels already include units */
    set_menu_active_editor_get_label(wifi_label_wrapper);
    set_menu_active_editor_on_save(action_save_wifi_selection);
    set_menu_active_editor_focus_index(EDITOR_FOCUS_PLUS);
    set_current_ui_state(UI_STATE_EDIT_VALUE);
    render_full(NULL, NULL);
}

/**
 * @brief Opens the Choose Server configuration editor UI.
 */
void action_open_choose_server_editor(void)
{
    set_menu_active_editor_title("Choose Server");

    int current_index = server_get_active_index();
    set_menu_active_editor_current_val(current_index);
    set_menu_active_editor_min_val(0);

    int max_val = server_get_profiles_count() - 1;
    if (max_val < 0)
        max_val = 0; // Fallback
    set_menu_active_editor_max_val(max_val);

    set_menu_active_editor_step(1);
    set_menu_active_editor_unit_suffix(""); /* Optional, labels already include units */
    set_menu_active_editor_get_label(server_label_wrapper);
    set_menu_active_editor_on_save(action_save_server_selection);
    set_menu_active_editor_focus_index(EDITOR_FOCUS_PLUS);
    set_current_ui_state(UI_STATE_EDIT_VALUE);
    render_full(NULL, NULL);
}


/**
 * @brief Returns the system view back to the splash/running screen.
 */
void return_to_splash_screen(void)
{
    set_current_ui_state(UI_STATE_SPLASH);
    reset_to_root_menu();
    render_full("RUNNING", NULL);
}

/**
 * @brief Prepares and renders the voice command screen state.
 */
void action_open_audio_command_screen(void)
{
    set_current_ui_state(UI_STATE_VOICE_RECORDING);
    render_full("Ready to record", "");
}
