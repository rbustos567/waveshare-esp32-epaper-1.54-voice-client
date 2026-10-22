#include "ui_control.h"

static const char *TAG = "UI_CONTROL";

static ui_state_t current_ui_state = UI_STATE_SPLASH;
static QueueHandle_t xButtonQueue = NULL;
static volatile bool g_cancel_voice_session = false;
static volatile bool g_is_waiting_response = false;

bool get_cancel_voice_session(void) {
    return g_cancel_voice_session; 
}

esp_err_t set_cancel_voice_session(bool state) {
    esp_err_t ret = ESP_OK;
    
    g_cancel_voice_session = state;

    return ret;
}

bool get_is_waiting_response(void) {
    return g_is_waiting_response;
}

esp_err_t set_is_waiting_response(bool state) {
    esp_err_t ret = ESP_OK;

    g_is_waiting_response = state;

    return ret;
}

void vButtonTask(void *pvParameters) {

    const TickType_t double_click_window = pdMS_TO_TICKS(300);
    TickType_t last_press_time = 0;
    int press_count = 0;
    int last_state = 1;

    for (;;) {
        int current_state = gpio_get_level(BOOT_BUTTON_GPIO);

        if (last_state == 1 && current_state == 0) {
            vTaskDelay(pdMS_TO_TICKS(30)); // Debounce
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                press_count++;
                last_press_time = xTaskGetTickCount();
            }
        }

        if (press_count > 0 && (xTaskGetTickCount() - last_press_time) > double_click_window) {
            button_event_t evt = EVENT_NONE;
            if (press_count == 1) {
                evt = EVENT_SINGLE_CLICK;
            } else if (press_count >= 2) {
                evt = EVENT_DOUBLE_CLICK;
            }

            if (evt != EVENT_NONE) {
                xQueueSend(xButtonQueue, &evt, portMAX_DELAY);
            }
            press_count = 0;
        }

        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void vMenuTask(void *pvParameters) {
    button_event_t evt;

    for (;;) {
        if (xQueueReceive(xButtonQueue, &evt, portMAX_DELAY) == pdTRUE) {
            // 1. REINICIAR TIMER DE INACTIVIDAD
            power_manager_reset_inactivity();	 
	    // Splash Screen
            if (current_ui_state == UI_STATE_SPLASH) {
		if (evt == EVENT_SINGLE_CLICK) {
		    //render_splash_to_buffer("RUNNING");
                    render_full("RUNNING", NULL);
		} else if (evt == EVENT_DOUBLE_CLICK) {
                    set_current_ui_state(UI_STATE_MENU);
                    reset_to_root_menu(); 
                    render_full(NULL, NULL);
                }
	    // Menu Screen
            } else if (current_ui_state == UI_STATE_MENU) {
                Menu *current_menu = get_current_menu();
                if (!current_menu)
	            continue;

                if (evt == EVENT_SINGLE_CLICK) {
                    current_menu->selected_index = (current_menu->selected_index + 1) % current_menu->num_items;
                    ESP_LOGI(TAG, "Moving to selected index: %d", current_menu->selected_index);
                    render_partial();
                } else if (evt == EVENT_DOUBLE_CLICK) {
                    MenuItem selected = current_menu->items[current_menu->selected_index];

                    if (selected.is_exit) {
                        if (get_menu_stack_top() > 0) {
                            current_menu->selected_index = 0; // Reset index when exit
                            pop_menu();
                            ESP_LOGD(TAG, "Back to previous Menu...");
                            render_full(NULL, NULL);
                        } else {
                            ESP_LOGD(TAG, "You are already in root Menu!");
                        }
                    } else if (selected.sub_menu != NULL) {
                        selected.sub_menu->selected_index = 0;
                        push_menu(selected.sub_menu);
                        ESP_LOGD(TAG, "Entering to subMenu: %s", selected.sub_menu->title);
                        render_full(NULL, NULL);
                    } else if (selected.action != NULL) {
                        selected.action();
                    }
                }
	    // Edit Screen
	    } else if (current_ui_state == UI_STATE_EDIT_VALUE) {
                
		int curr_focus;
		curr_focus = get_menu_active_editor_focus_index();
                
                if (evt == EVENT_SINGLE_CLICK) {
		    curr_focus = (curr_focus + 1) % EDITOR_FOCUS_MAX;

		    set_menu_active_editor_focus_index(curr_focus);
		    render_partial(); 
		} else if (evt == EVENT_DOUBLE_CLICK) {

		    ESP_LOGI(TAG, "Double Click on UI_STATE_EDIT_VALUE"); 
		    int curr_val, min, max, step;
		    curr_val = get_menu_active_editor_current_val();

	            switch (curr_focus) {
                        case EDITOR_FOCUS_PLUS:
			    step = get_menu_active_editor_step();
			    max = get_menu_active_editor_max_val();
			    min = get_menu_active_editor_min_val();
                            curr_val += step;
                            if (curr_val > max)
                                set_menu_active_editor_current_val(min); // Rollover o límite
                            else
                                set_menu_active_editor_current_val(curr_val);

                            render_partial();
                            break;

                        case EDITOR_FOCUS_MINUS:
                            step = get_menu_active_editor_step();
                            max = get_menu_active_editor_max_val();
                            min = get_menu_active_editor_min_val();
                            curr_val -= step;
                            if (curr_val < min)
                                set_menu_active_editor_current_val(max); // Rollover o límite
			    else
                                set_menu_active_editor_current_val(curr_val);

                            render_partial();
                            break;

                        case EDITOR_FOCUS_SAVE:
			    ESP_LOGI(TAG, "Before get_menu_active_editor_on_save");
			    void (*save_cb)(int) = get_menu_active_editor_on_save();

			    if (save_cb != NULL) {
				ESP_LOGI(TAG, "Before calling callback");
                                save_cb(curr_val); 
			    }
                            //if (editor->on_save != NULL) {
                            //    editor->on_save(editor->current_val); // Llama al callback setter (RAM/NVS)
                            //}
			    ESP_LOGI(TAG, "Before changing to UI_STATE_MENU");
                            set_current_ui_state(UI_STATE_MENU);
			    ESP_LOGI(TAG, "Before render_full");
                            render_full(NULL, NULL);
                            break;

                        case EDITOR_FOCUS_EXIT:
                            // Sale sin guardar los cambios
                            set_current_ui_state(UI_STATE_MENU);
                            render_full(NULL, NULL);
                            break;
                    }   
                }
	    // Voice command Workflow
	    } else if (current_ui_state == UI_STATE_VOICE_RECORDING) {
		
		if (!get_is_waiting_response()) {

                    if (evt == EVENT_SINGLE_CLICK) {
		        ESP_LOGI(TAG, "Cancelación detectada. Volviendo al menú principal...");

	                action_cancel_voice_session();
		        //vTaskDelay(pdMS_TO_TICKS(300));
                        ESP_LOGI(TAG, "Regresando a Menu principal...");
		        Menu *current_menu = get_current_menu();
                        current_menu->selected_index = 0; // Reset index when exit
                        set_current_ui_state(UI_STATE_MENU);
	            
		        render_full(NULL, NULL); 
                    } else if (evt == EVENT_DOUBLE_CLICK) {
                        //render_partial_with_text("Recording...", NULL);
		        action_record_and_send_audio(); 
	            }
	        } else {
	            ESP_LOGI(TAG, "Esperando respuesta del server, ignorando click...");
	        }	    
            }
        }
    }
}

esp_err_t ui_control_init(void) {
    esp_err_t ret = ESP_OK;

    ret = button_hardware_init(BOOT_BUTTON_GPIO);
    if (ret == ESP_OK) {

        xButtonQueue = xQueueCreate(UI_BUTTON_QUEUE_LEN, sizeof(button_event_t));
        if (xButtonQueue != NULL) {

            BaseType_t res1 = xTaskCreate(
	        vButtonTask, 
		UI_BUTTON_TASK_NAME, 
		UI_BUTTON_TASK_STACK_SIZE, 
		NULL, 
		UI_BUTTON_TASK_PRIO, 
		NULL);

            BaseType_t res2 = xTaskCreate(
	        vMenuTask, 
		UI_MENU_TASK_NAME, 
		UI_MENU_TASK_STACK_SIZE, 
		NULL, 
		UI_MENU_TASK_PRIO, 
		NULL);

            if (res1 != pdPASS || res2 != pdPASS) {
                ret = ESP_FAIL;
            }
        } else {
            ret = ESP_FAIL;
        }
    }

    return ret;
}

ui_state_t get_current_ui_state(void) {
    return current_ui_state;
}

esp_err_t set_current_ui_state(ui_state_t ui_state) {
    esp_err_t ret = ESP_OK;

    if (current_ui_state < 0 || current_ui_state >= UI_STATE_MAX) {
        ret = ESP_ERR_INVALID_ARG;
    } else {
        current_ui_state = ui_state;
    }

    return ret;
}
