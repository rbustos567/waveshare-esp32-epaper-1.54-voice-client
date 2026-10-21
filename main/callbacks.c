#include "callbacks.h"

static const char *TAG = "CALLBACKS";

void action_demo(void) {

}

static void voice_session_task(void *pvParameters) {
    ESP_LOGI(TAG, "Iniciando sesión de voz...");

    // 1. ESTADO: Grabando audio
    // Actualizamos la pantalla visualmente
    render_partial_with_text("Recording...", "Speak your command...");

    // Iniciamos la grabación (primer doble clic)
    audio_manager_toggle_recording();

    // Esperar mientras el usuario graba (hasta el segundo doble clic)
    while (audio_manager_is_recording()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 2. ESTADO: Enviando HTTP request
    if (audio_manager_get_recorded_bytes() > 0) {
        
	ESP_LOGI(TAG, "Enviando audio por HTTP...");

        render_partial_with_text("Sending to server...", "Uploading audio to server...");

	char server_response_buf[256] = {0};

        // Ejecutamos la petición HTTP (bloquea brevemente hasta recibir respuesta)
        api_client_send_audio(server_response_buf, sizeof(server_response_buf));

        const char * server_response = api_client_get_remote_cmd_output();

	if (strlen(server_response) > 0) {
            render_partial_with_text("Ready to record", server_response);
        } else {
            render_partial_with_text("Ready to record", "Done.");
        }

	//const char *cmd_res = api_client_get_last_cmd_output();
        //if (cmd_res && strlen(cmd_res) > 0) {
	//    render_partial_with_text("Ready to record", cmd_res);
	//} else {
        //    render_partial_with_text("Ready to record", "Command executed.");
        //    vTaskDelay(pdMS_TO_TICKS(5000)); // Dejar el resultado visible por 5 segundos
        //}
    } else {
        ESP_LOGW(TAG, "No se grabó audio.");
    }

    // 3. Finalizar y regresar al menú principal
//    vTaskDelay(pdMS_TO_TICKS(1000)); // Pausa breve de 1s para que alcance a leer el mensaje de envío
//    set_current_ui_state(UI_STATE_MENU);
//    render_full(NULL, NULL); // Restaura la vista del menú

    vTaskDelete(NULL);
}

void action_record_and_send_audio(void) {
    if (!audio_manager_is_recording()) {
        // Cambiamos el estado de la UI para bloquear interacciones del menú
        set_current_ui_state(UI_STATE_VOICE_RECORDING);
        
        // Lanzamos la tarea de orquestación en el Core 1
        xTaskCreatePinnedToCore(
            voice_session_task,
            "voice_session_task",
            4096,
            NULL,
            5,
            NULL,
            1
        );
    } else {
        // Segundo doble clic: detiene la grabación en curso
        audio_manager_toggle_recording();
    }
}

void action_record_audio(void) {
    audio_manager_toggle_recording();
}

void action_listen_audio(void) {

    audio_manager_play_recorded_audio();
}

// void action_send_audio(void) {
//    api_client_send_audio();    
//}

void action_save_timezone(int selected_index) {
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Before set_current_tz_index");
    set_current_tz_index(selected_index);

    // 1. Guardar en NVS
    nvs_handle_t nvs_h;
    if (nvs_open("storage", NVS_READWRITE, &nvs_h) == ESP_OK) {
        nvs_set_i32(nvs_h, "tz_idx", selected_index);
        nvs_commit(nvs_h);
        nvs_close(nvs_h);
    }

    // 2. Aplicar inmediatamente al sistema C / ESP-IDF
    tz_option_t *tz_option = timezone_get_option_at(selected_index);
    if (tz_option != NULL && tz_option->tz_string != NULL) {
        setenv("TZ", tz_option->tz_string, 1);
        tzset();
	ESP_LOGI(TAG, "New TimeZone set: %s", tz_option->tz_string);
    }

//    ESP_LOGI("CALLBACKS", "Completed!");
}

void action_open_tz_editor(void) {
   
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

void action_save_sleep_timeout(int selected_index) {
    power_manager_set_timeout_index(selected_index);
}

void action_open_sleep_timeout_editor(void) {

    set_menu_active_editor_title("Set Sleep Timeout");

    int timeout_index = power_manager_get_current_timeout_index();
    set_menu_active_editor_current_val(timeout_index);
    set_menu_active_editor_min_val(0);

    int max_val = (int) power_manager_get_timeout_options_count() - 1;
    set_menu_active_editor_max_val(max_val);

    set_menu_active_editor_step(1);
    set_menu_active_editor_unit_suffix(""); // Opcional, las etiquetas ya incluyen "s"/"min"
    set_menu_active_editor_get_label(get_timeout_label_wrapper);
    set_menu_active_editor_on_save(action_save_sleep_timeout);
    set_menu_active_editor_focus_index(EDITOR_FOCUS_PLUS);
    set_current_ui_state(UI_STATE_EDIT_VALUE);
    render_full(NULL, NULL);
}

void return_to_splash_screen(void) {

    set_current_ui_state(UI_STATE_SPLASH);
    reset_to_root_menu();
    //render_splash_to_buffer();
    render_full("RUNNING", NULL);
}

void action_open_audio_command_screen(void) {
    set_current_ui_state(UI_STATE_VOICE_RECORDING);
    render_full("Ready to record", "");
}
