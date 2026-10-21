#ifndef UI_MENU_MANAGER_H
#define UI_MENU_MANAGER_H

#include <stdint.h>  
#include <stdbool.h> 
#include <stddef.h> 
#include "DEV_Config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STACK_DEPTH 8
#define ROOT_STACK_DEPTH 0

typedef struct {
    const char *title;      // Ej: "Set TimeZone"
    int current_val;        // Valor que está cambiando el usuario temporalmente
    int min_val;            // Ej: 0
    int max_val;            // Ej: 100
    int step;               // Ej: 5
    const char *unit_suffix;// Ej: "%" o "s"
    const char* (*get_label)(int val); // <-- Opcional: Para editores basados en listas de texto (Timezone, Timeout, etc.) 
    void (*on_save)(int val); // Callback que se ejecuta al darle a "Save" (guarda en NVS/RAM)
    int focus_index; // <-- Guarda qué control está seleccionado (0: +, 1: -, 2: Save, 3: Exit)
} ValueEditor;

typedef enum {
    EDITOR_FOCUS_PLUS = 0,
    EDITOR_FOCUS_MINUS,
    EDITOR_FOCUS_SAVE,
    EDITOR_FOCUS_EXIT,
    EDITOR_FOCUS_MAX
} editor_focus_t;

typedef struct {
    const char *title;
    struct Menu *sub_menu;
    void (*action)(void);
    bool is_exit;
    void (*get_value_str)(char *dest, size_t max_len);
} MenuItem;

typedef struct Menu {
    const char *title;
    MenuItem *items;
    size_t num_items;
    int selected_index;
} Menu;

esp_err_t push_menu(Menu *menu);
Menu* pop_menu(void);
Menu* get_current_menu(void);
esp_err_t init_menu(void);
int get_menu_stack_top(void);
esp_err_t set_menu_stack_top(int idx);
esp_err_t reset_to_root_menu(void);
esp_err_t set_menu_active_editor_title(const char *title);
esp_err_t set_menu_active_editor_current_val(int val);
esp_err_t set_menu_active_editor_min_val(int val);
esp_err_t set_menu_active_editor_max_val(int val);
esp_err_t set_menu_active_editor_step(int step);
esp_err_t set_menu_active_editor_unit_suffix(const char *unit_suffix);
esp_err_t set_menu_active_editor_on_save(void (*cb)(int val));
esp_err_t set_menu_active_editor_focus_index(int index);
const char *get_menu_active_editor_title(void);
int get_menu_active_editor_current_val(void);
int get_menu_active_editor_min_val(void);
int get_menu_active_editor_max_val(void);
int get_menu_active_editor_step(void);
int get_menu_active_editor_focus_index(void);
const char *get_menu_active_editor_unit_suffix(void);
void (*get_menu_active_editor_on_save(void))(int);
const char* (*get_menu_active_editor_get_label(void))(int);
esp_err_t set_menu_active_editor_get_label(const char* (*get_label_cb)(int val));

#ifdef __cplusplus
}
#endif

#endif // UI_MENU_MANAGER_H
