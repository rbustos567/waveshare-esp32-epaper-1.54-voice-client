#include "ui_renderer.h"

static const char *TAG = "UI_RENDERER";

esp_err_t render_draw_battery_icon_top_right(int percentage) {
    esp_err_t ret = ESP_OK;

    // Posición X calculada para alineación derecha en 200px
    // Ancho total del componente: Batería (24px) + Borne (2px) + Espacio (4px) + Texto 100% (~32px) = ~62px
    UWORD start_x = 170;
    UWORD start_y = 8;
    UWORD width = 20;
    UWORD height = 10;

    // 1. Cuerpo de la batería
    Paint_DrawRectangle(start_x, start_y, start_x + width, start_y + height, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

    // 2. Pila
    Paint_DrawRectangle(start_x + width, start_y + 3, start_x + width + 2, start_y + height - 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY );
    
    // 3. Nivel de carga interno
    UWORD fill_width = ((width - 4) * percentage) / 100;
    if (fill_width > 0)
        Paint_DrawRectangle(start_x + 2, start_y + 2, start_x + 2 + fill_width, start_y + height - 1, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    // 4. Texto directo del porcentaje (ej. "100%" o "85%")
    char bstr[8];
    UWORD alig_x = 32;
    snprintf(bstr, sizeof(bstr), "%d%%", percentage);
    if (percentage <= 99 && percentage >= 9) {
        alig_x = 24;
    } else if (percentage <= 9 && percentage >= 0) {
        alig_x = 18;
    }
    Paint_DrawString_EN(start_x - alig_x, start_y, bstr, &Font12, BLACK, WHITE);

    return ret;
}

esp_err_t render_splash_to_buffer(const char *status_msg) {
    esp_err_t ret = ESP_OK;

    // Limpiar buffer (Fondo blanco)
    Paint_Clear(WHITE);

    // -------------------------------------------------------------------------
    // ENCABEZADO: Fecha (Izquierda) y Batería (Derecha) | Y: 0 a 24
    // -------------------------------------------------------------------------
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // 1. Fecha en la Esquina Superior Izquierda
    char date_str[16];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
    Paint_DrawString_EN(8, 8, date_str, &Font12, BLACK, WHITE);

    // 2. Batería en la Esquina Superior Derecha (Sin "BAT:")
    int battery_pct = battery_get_percentage();
    render_draw_battery_icon_top_right(battery_pct);

    // Línea separadora horizontal del encabezado
    Paint_DrawLine(1, 24, 199, 24, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // -------------------------------------------------------------------------
    // BLOQUE PRINCIPAL: Hora Centrada | Y: 25 a 100
    // -------------------------------------------------------------------------
    char time_str[16];
    char tz_code[8];
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
    strftime(tz_code, sizeof(tz_code), "%Z", &timeinfo);

    ESP_LOGI(TAG, "Epoch: %lld | Hora Local: %02d:%02d | TZ env: %s", 
         (long long)now, timeinfo.tm_hour, timeinfo.tm_min, getenv("TZ"));

    // Hora grande en Font24 (~85px de ancho, centrada desde X=57)
    Paint_DrawString_EN(57, 40, time_str, &Font24, BLACK, WHITE);

    // Indicador de zona horaria / etiqueta secundaria
    Paint_DrawString_EN(86, 70, tz_code, &Font12, BLACK, WHITE);

    // Línea separadora media
    Paint_DrawLine(1, 100, 199, 100, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // -------------------------------------------------------------------------
    // PIE DE PÁGINA: Uptime del Sistema | Y: 101 a 200
    // -------------------------------------------------------------------------
    Paint_DrawString_EN(12, 112, "SYSTEM UPTIME", &Font12, BLACK, WHITE);

    // Uptime acumulado persistente
    char uptime_str[32];
    uptime_get_formatted(uptime_str, sizeof(uptime_str));
    Paint_DrawString_EN(12, 132, uptime_str, &Font16, BLACK, WHITE);

    // Estado general del sistema
    Paint_DrawString_EN(12, 172, "STATUS:", &Font12, BLACK, WHITE);
    if (status_msg)
        Paint_DrawString_EN(68, 172, status_msg, &Font12, BLACK, WHITE);

    return ret;
}

esp_err_t render_voice_screen_to_buffer(const char *status_msg, const char *server_output_msg) {
    esp_err_t ret = ESP_OK;

    // Limpiar buffer (Fondo blanco)
    Paint_Clear(WHITE);

    // ----------------------------------------------------
    // 1. ENCABEZADO: Estado del Sistema | Y: 0 a 24
    // ----------------------------------------------------
    Paint_DrawString_EN(8, 8, "STATUS:", &Font12, BLACK, WHITE);
    if (status_msg) {
        Paint_DrawString_EN(60, 8, (char *)status_msg, &Font12, BLACK, WHITE);
    }

    // Línea separadora horizontal del encabezado
    Paint_DrawLine(1, 24, 199, 24, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // ----------------------------------------------------
    // 2. BLOQUE PRINCIPAL: Resultado / Output del Server | Y: 25 a 170
    // ----------------------------------------------------
    Paint_DrawString_EN(12, 35, "SERVER RESPONSE:", &Font12, BLACK, WHITE);

    if (server_output_msg && strlen(server_output_msg) > 0) {
        // Pintamos el mensaje o la salida del comando centrada/ajustada en el cuerpo
        Paint_DrawString_EN(12, 60, (char *)server_output_msg, &Font12, BLACK, WHITE);
    } else {
        Paint_DrawString_EN(12, 60, "", &Font12, BLACK, WHITE);
    }

    // Línea separadora inferior
    Paint_DrawLine(1, 168, 199, 168, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // ----------------------------------------------------
    // 3. PIE DE PÁGINA: Guía de Botones (Esquinas) | Y: 176 a 200
    // ----------------------------------------------------
    // Renglón superior: 2-Click para Grabar/Parar
    Paint_DrawString_EN(8, 172, "2-Click: Rec/Stop", &Font12, BLACK, WHITE);

    // Renglón inferior: 1-Click para Salir
    Paint_DrawString_EN(8, 186, "1-Click: Exit", &Font12, BLACK, WHITE);

    return ret;
}

esp_err_t render_edit_screen_to_buffer(void) {
    esp_err_t ret = ESP_OK;

    Paint_Clear(WHITE);
    const char *title = get_menu_active_editor_title();
    Paint_DrawString_EN(10, 6, title, UI_MENU_TITLE_FONT, BLACK, WHITE);
    Paint_DrawLine(1, HEADER_HEIGHT, EPD_WIDTH - 1, HEADER_HEIGHT, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    int focus_index = get_menu_active_editor_focus_index();

    // [+]
    int i = 0;
    int y_pos = HEADER_HEIGHT + 8 + (i * ITEM_HEIGHT);
    bool is_sel = (focus_index == EDITOR_FOCUS_PLUS);
    Paint_DrawString_EN(10, y_pos + 3, "[ + ]", UI_MENU_ITEM_FONT, is_sel ? WHITE : BLACK, is_sel ? BLACK : WHITE);

    // Value
    char val_str[32];
    i++;
    y_pos = HEADER_HEIGHT + 8 + (i * ITEM_HEIGHT);
    const char *unit_suffix = get_menu_active_editor_unit_suffix();  
    int curr_val = get_menu_active_editor_current_val();

    // Obtener la función formateadora
    const char* (*get_label)(int) = get_menu_active_editor_get_label();

    if (get_label != NULL) {
        // Para elementos basados en listas/lookup tables (Timezone, Sleep Timeout)
        snprintf(val_str, sizeof(val_str), "%s", get_label(curr_val));
    } else if (unit_suffix != NULL && strlen(unit_suffix) > 0) {
        // Para valores numéricos con sufijo (ej: "80 %")
        snprintf(val_str, sizeof(val_str), "%d %s", curr_val, unit_suffix);
    } else {
        // Para enteros simples sin sufijo (ej: "10")
        snprintf(val_str, sizeof(val_str), "%d", curr_val);
    }

    Paint_DrawString_EN(10, y_pos + 3, val_str, UI_MENU_ITEM_FONT, BLACK, WHITE);

    // [-]
    i++;
    y_pos = HEADER_HEIGHT + 8 + (i * ITEM_HEIGHT);
    is_sel = (focus_index == EDITOR_FOCUS_MINUS);
    Paint_DrawString_EN(10, y_pos + 3, "[ - ]", UI_MENU_ITEM_FONT, is_sel ? WHITE : BLACK, is_sel ? BLACK : WHITE);

    // Save
    i++;
    y_pos = HEADER_HEIGHT + 8 + (i * ITEM_HEIGHT);
    is_sel = (focus_index == EDITOR_FOCUS_SAVE);
    Paint_DrawString_EN(10, y_pos + 3, "Save and Exit", UI_MENU_ITEM_FONT, is_sel ? WHITE : BLACK, is_sel ? BLACK : WHITE);

    // Exit
    i++;
    y_pos = HEADER_HEIGHT + 8 + (i * ITEM_HEIGHT);
    is_sel = (focus_index == EDITOR_FOCUS_EXIT);
    Paint_DrawString_EN(10, y_pos + 3, "Exit", UI_MENU_ITEM_FONT, is_sel ? WHITE : BLACK, is_sel ? BLACK : WHITE);

    return ret;
}
esp_err_t render_menu_to_buffer(Menu *current_menu) {
    esp_err_t ret = ESP_OK;

    Paint_Clear(WHITE);
    Paint_DrawString_EN(10, 6, current_menu->title, UI_MENU_TITLE_FONT, BLACK, WHITE);
    Paint_DrawLine(1, HEADER_HEIGHT, EPD_WIDTH - 1, HEADER_HEIGHT, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    for (size_t i = 0; i < current_menu->num_items; i++) {
        int y_pos = HEADER_HEIGHT + 8 + (i * ITEM_HEIGHT);

        if ((int)i == current_menu->selected_index) {
            Paint_DrawString_EN(10, y_pos + 3, current_menu->items[i].title, UI_MENU_ITEM_FONT, WHITE, BLACK);
        } else {
            Paint_DrawString_EN(10, y_pos + 3, current_menu->items[i].title, UI_MENU_ITEM_FONT, BLACK, WHITE);
        }
    }

    return ret;
}

esp_err_t render(const char *st_msg, const char *output) {
    esp_err_t ret = ESP_OK;

    ui_state_t ui_state = get_current_ui_state();

    switch (ui_state) {
        case UI_STATE_SPLASH:
            ret = render_splash_to_buffer(st_msg);
            break;
    
        case UI_STATE_MENU:
            Menu *current = get_current_menu();
            if (!current)
                return ESP_ERR_INVALID_ARG;
    
            ret = render_menu_to_buffer(current);
            if (ret != ESP_OK)
                return ret;
            break;
	
	case UI_STATE_EDIT_VALUE:
	    ret = render_edit_screen_to_buffer(); 
	    break;
   
	case UI_STATE_VOICE_RECORDING:
	    ret = render_voice_screen_to_buffer(st_msg, output);
	    break;

        default:
            ESP_LOGE(TAG, "Incorrect ui_state: %d", ui_state);
            set_current_ui_state(UI_STATE_SPLASH);
            break;
    }

    return ret;
}

esp_err_t render_full(const char *status_msg, const char *output_msg) {
    esp_err_t ret = ESP_OK;

    ret = render(status_msg, output_msg);
    if (ret != ESP_OK)
        return ret;

    ret = epd_display_update_full();
    if (ret != ESP_OK)
        return ret;

    return ret;
}

esp_err_t render_partial_with_text(const char *status_msg, const char *output_msg) {
    esp_err_t ret = ESP_OK;

    ret = render(status_msg, output_msg);
    if (ret != ESP_OK)
        return ret;
    
    ESP_LOGI(TAG, "Ejecutando render_partial con nuevos mensajes...");
    ret = epd_display_update_partial();

    return ret;
}

esp_err_t render_partial(void) {
    esp_err_t ret = ESP_OK; 

    ret = render(NULL, NULL);
    if (ret != ESP_OK)
        return ret;

    ESP_LOGI(TAG, "render_partial"); 
    ret = epd_display_update_partial();

    return ret;
}

esp_err_t render_message_to_buffer(const char *message, UWORD x, UWORD y) {
    esp_err_t ret = ESP_OK;

    if (message == NULL) 
        return ESP_ERR_INVALID_ARG;

    Paint_Clear(WHITE);
    Paint_DrawString_EN(x, y, message, UI_MENU_ITEM_FONT, BLACK, WHITE);
    
    ret = epd_display_update_full();
    if (ret != ESP_OK)
        return ret;

    return ret;
}

esp_err_t render_init(void) {
    esp_err_t ret = ESP_OK;

    ret = epd_display_init();
    if (ret != ESP_OK)
        return ret;

    uint8_t *buf = epd_display_get_buffer();
    if (buf == NULL)
        ret = ESP_ERR_NO_MEM;

    Paint_NewImage(buf, EPD_1IN54_V2_WIDTH, EPD_1IN54_V2_HEIGHT, 0, WHITE);
    Paint_SelectImage(buf);
    Paint_Clear(WHITE);

    return ret;
}
