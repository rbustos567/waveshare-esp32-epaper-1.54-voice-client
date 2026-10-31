#include "ui_menu_manager.h"
#include "callbacks.h"

static const char *TAG = "UI_MENU_MANAGER";

static Menu *menu_stack[MAX_STACK_DEPTH];
static int stack_top = -1;
static ValueEditor s_active_editor;

static Menu main_menu;
static Menu sub_menu_config;
static Menu sub_menu_power_mgmt;
static Menu sub_menu_time_set;
static Menu sub_menu_connectivity;

static MenuItem connectivity_items[] = {
    {"Choose Network", NULL, action_open_choose_network_editor, false, NULL},
    {"Choose Server", NULL, action_open_choose_server_editor, false, NULL},
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
    {"Connectivity", &sub_menu_connectivity, NULL, false, NULL},
    {"Exit", NULL, NULL, true, NULL}
};

static MenuItem main_items[] = {
    {"Audio Command", NULL, action_open_audio_command_screen, false, NULL},
    {"Configuration", &sub_menu_config, NULL, false, NULL},
    {"Exit", NULL, return_to_splash_screen, false, NULL}
};

esp_err_t push_menu(Menu *menu) {
    esp_err_t ret = ESP_OK;

    if (stack_top < MAX_STACK_DEPTH - 1) {
        stack_top++;
        menu_stack[stack_top] = menu;
	ESP_LOGD(TAG, "Menu pushed to stack. Top: %d, Title: %s", stack_top, menu ? menu->title : "NULL");
    } else {
	ESP_LOGW(TAG, "Stack overflow prevented: Max depth (%d) reached", MAX_STACK_DEPTH);
        ret = ESP_ERR_INVALID_ARG; 
    }

    return ret;
}

Menu* pop_menu(void) {
    
    if (stack_top >= 0) {
        Menu *m = menu_stack[stack_top];
        stack_top--;
	ESP_LOGD(TAG, "Menu popped from stack. New top: %d", stack_top);
        return m;
    }
    
    ESP_LOGW(TAG, "Stack underflow: Tried to pop from empty stack (top = %d)", stack_top);
    return NULL;
}

Menu* get_current_menu(void) {
    
    if (stack_top >= 0)
        return menu_stack[stack_top];

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
	    ESP_LOGI(TAG, "Navigation reset back to root menu");
	}
    } else {
        ESP_LOGW(TAG, "Failed to reset to root menu, error code: %d", ret);
    }

    return ret;
}

esp_err_t init_menu(void) {
    esp_err_t ret = ESP_OK;

    sub_menu_connectivity.title = "Connectivity";
    sub_menu_connectivity.items = connectivity_items;
    sub_menu_connectivity.num_items = sizeof(connectivity_items) / sizeof(MenuItem);
    sub_menu_connectivity.selected_index = 0;

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
                       
    main_menu.title = "Main Menu";
    main_menu.items = main_items;
    main_menu.num_items = sizeof(main_items) / sizeof(MenuItem);
    main_menu.selected_index = 0;
    
    push_menu(&main_menu);

    ESP_LOGI(TAG, "Menu manager initialized successfully. Main menu loaded.");

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
    esp_err_t ret = ESP_OK;

    s_active_editor.get_label = get_label_cb;

    return ret;
}
