#include "ui_menu_manager.h"
#include "callbacks.h"

static Menu *menu_stack[MAX_STACK_DEPTH];
static int stack_top = -1;
static ValueEditor s_active_editor;

static Menu main_menu;
static Menu sub_menu_config;
static Menu sub_menu_info;
static Menu sub_menu_power_mgmt;
static Menu sub_menu_time_set;
static Menu sub_menu_audio_set;
//static Menu sub_menu_actions;

static MenuItem audio_set_items[] = {
    {"Set Volume", NULL, action_demo, false, NULL},
    {"Exit", NULL, NULL, true, NULL}
};

static MenuItem time_set_items[] = {
    {"Set TimeZone", NULL, action_open_tz_editor, false, NULL},
    {"Exit", NULL, NULL, true, NULL}
};

static MenuItem power_mgmt_items[] = {
    {"Set Sleep Timeout", NULL, action_open_sleep_timeout_editor, false, NULL},
    {"Exit", NULL, NULL, true, NULL}
};

static MenuItem config_items[] = {
    {"Power Management", &sub_menu_power_mgmt, NULL, false, NULL},
    {"Time Settings", &sub_menu_time_set, NULL, false, NULL},
    {"Audio Settings", &sub_menu_audio_set, NULL, false, NULL},
    {"Exit", NULL, NULL, true, NULL}
};

static MenuItem info_items[] = {
    {"Version FW", NULL, action_demo, false, NULL},
    {"Exit", NULL, NULL, true, NULL}
};

/*
static MenuItem actions_items[] = {
    {"Audio Command", NULL, action_record_and_send_audio, false, NULL}, 
    {"Record Audio", NULL, action_record_audio, false, NULL},
    {"Play Audio", NULL, action_listen_audio, false, NULL},
    {"Send Audio", NULL, action_send_audio, false, NULL},
    {"Exit", NULL, NULL, true, NULL}
};
*/

static MenuItem main_items[] = {
    {"Audio Command", NULL, action_open_audio_command_screen, false, NULL},
    {"Configuration", &sub_menu_config, NULL, false, NULL},
    {"Information", &sub_menu_info, NULL, false, NULL},
    {"Exit", NULL, return_to_splash_screen, false, NULL}
};

esp_err_t push_menu(Menu *menu) {
    esp_err_t ret = ESP_OK;

    if (stack_top < MAX_STACK_DEPTH - 1) {
        stack_top++;
        menu_stack[stack_top] = menu;
    } else {
        ret = ESP_ERR_INVALID_ARG; 
    }

    return ret;
}

Menu* pop_menu(void) {
    if (stack_top >= 0) {
        Menu *m = menu_stack[stack_top];
        stack_top--;
        return m;
    }
    return NULL;
}

Menu* get_current_menu(void) {
    if (stack_top >= 0) {
        return menu_stack[stack_top];
    }
    return NULL;
}

int get_menu_stack_top(void) {
    return stack_top;
}

esp_err_t set_menu_stack_top(int idx) {
    esp_err_t ret = ESP_OK;
    
    if (idx < ROOT_STACK_DEPTH || idx >= MAX_STACK_DEPTH)
        ret = ESP_ERR_INVALID_ARG;
    else
        stack_top = idx;

    return ret;
}

esp_err_t reset_to_root_menu(void) {
    esp_err_t ret = ESP_OK;

    ret = set_menu_stack_top(ROOT_STACK_DEPTH);
    if (ret == ESP_OK) {
        Menu *root_menu = menu_stack[stack_top];
	if (root_menu != NULL) {
	    root_menu->selected_index = 0;
	}
    }

    return ret;
}

esp_err_t init_menu(void) {
    esp_err_t ret = ESP_OK;

//    sub_menu_actions.title = "Actions";
//    sub_menu_actions.items = actions_items;
//    sub_menu_actions.num_items = sizeof(actions_items) / sizeof(MenuItem);
//    sub_menu_actions.selected_index = 0;

    sub_menu_audio_set.title = "Audio Settings";
    sub_menu_audio_set.items = audio_set_items;
    sub_menu_audio_set.num_items = sizeof(audio_set_items) / sizeof(MenuItem);
    sub_menu_audio_set.selected_index = 0;

    sub_menu_time_set.title = "Time Settings";
    sub_menu_time_set.items = time_set_items;
    sub_menu_time_set.num_items = sizeof(time_set_items) / sizeof(MenuItem);
    sub_menu_time_set.selected_index = 0;

    sub_menu_power_mgmt.title = "Power Management";
    sub_menu_power_mgmt.items = power_mgmt_items;
    sub_menu_power_mgmt.num_items = sizeof(power_mgmt_items) / sizeof(MenuItem);
    sub_menu_power_mgmt.selected_index = 0;

    sub_menu_config.title = "Configuration";
    sub_menu_config.items = config_items;
    sub_menu_config.num_items = sizeof(config_items) / sizeof(MenuItem);
    sub_menu_config.selected_index = 0;
                       
    sub_menu_info.title = "Information";
    sub_menu_info.items = info_items;
    sub_menu_info.num_items = sizeof(info_items) / sizeof(MenuItem);
    sub_menu_info.selected_index = 0;
        
    main_menu.title = "Main Menu";
    main_menu.items = main_items;
    main_menu.num_items = sizeof(main_items) / sizeof(MenuItem);
    main_menu.selected_index = 0;
    
    push_menu(&main_menu);

    return ret;
}

esp_err_t set_menu_active_editor_title(const char *title) {
    esp_err_t ret = ESP_OK;

    s_active_editor.title = title;

    return ret;
}

esp_err_t set_menu_active_editor_current_val(int val) {
    esp_err_t ret = ESP_OK;

    s_active_editor.current_val = val;

    return ret;
}

esp_err_t set_menu_active_editor_min_val(int val) {
    esp_err_t ret = ESP_OK;

    s_active_editor.min_val = val;

    return ret;
}

esp_err_t set_menu_active_editor_max_val(int val) {
    esp_err_t ret = ESP_OK;

    s_active_editor.max_val = val;

    return ret;
}

esp_err_t set_menu_active_editor_step(int step) {
    esp_err_t ret = ESP_OK;

    s_active_editor.step = step;

    return ret;
}

esp_err_t set_menu_active_editor_unit_suffix(const char *unit_suffix) {
    esp_err_t ret = ESP_OK;

    s_active_editor.unit_suffix = unit_suffix; 

    return ret;
}

esp_err_t set_menu_active_editor_on_save(void (*cb)(int val)) {
    esp_err_t ret = ESP_OK;

    s_active_editor.on_save = cb;
    return ret;
}

esp_err_t set_menu_active_editor_focus_index(int index) {
    esp_err_t ret = ESP_OK;

    s_active_editor.focus_index = index;
    return ret;
}

const char *get_menu_active_editor_title(void) {
    return s_active_editor.title;
}

int get_menu_active_editor_current_val(void) {
    return s_active_editor.current_val;    
}

int get_menu_active_editor_max_val(void) {
    return s_active_editor.max_val;
}

int get_menu_active_editor_min_val(void) {
    return s_active_editor.min_val;
}

int get_menu_active_editor_step(void) {
    return s_active_editor.step;
}

int get_menu_active_editor_focus_index(void) {
    return s_active_editor.focus_index;
}

const char *get_menu_active_editor_unit_suffix(void) {
    return s_active_editor.unit_suffix;    
}

void (*get_menu_active_editor_on_save(void))(int) {
    return s_active_editor.on_save;
}

const char* (*get_menu_active_editor_get_label(void))(int) {
    return s_active_editor.get_label;
}

esp_err_t set_menu_active_editor_get_label(const char* (*get_label_cb)(int val)) {
    s_active_editor.get_label = get_label_cb;
    return ESP_OK;
}
