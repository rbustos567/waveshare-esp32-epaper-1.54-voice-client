#include "ui_renderer.h"

static const char *TAG = "UI_RENDERER";

esp_err_t render_draw_battery_icon_top_right(int percentage) {
    esp_err_t ret = ESP_OK;

    if (percentage < 0 || percentage > 100) {
        ESP_LOGW(TAG, "Battery percentage out of bounds (%d), clamping to 0-100", percentage);
        if (percentage < 0)
	    percentage = 0;

        if (percentage > 100) 
	    percentage = 100;
    }

    /* Calculated X position for right alignment in 200px */
    /* Total component width: Battery (24px) + Terminal (2px) + Spacing (4px) + Text 100% (~32px) = ~62px */
    UWORD start_x = 170;
    UWORD start_y = 8;
    UWORD width = 20;
    UWORD height = 10;

    // 1. Battery body
    Paint_DrawRectangle(start_x, start_y, start_x + width, start_y + height, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

    // 2. Battery Terminal
    Paint_DrawRectangle(start_x + width, start_y + 3, start_x + width + 2, start_y + height - 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY );
    
    // 3. Internal fill level
    UWORD fill_width = ((width - 4) * percentage) / 100;
    if (fill_width > 0)
        Paint_DrawRectangle(start_x + 2, start_y + 2, start_x + 2 + fill_width, start_y + height - 1, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    // 4. Direct percentage text (e.g. "100%" or "85%")
    char bstr[8];
    UWORD alig_x = 32;
    snprintf(bstr, sizeof(bstr), "%d%%", percentage);
    if (percentage <= 99 && percentage >= 9) {
        alig_x = 24;
    } else if (percentage <= 9 && percentage >= 0) {
        alig_x = 18;
    }
    Paint_DrawString_EN(start_x - alig_x, start_y, bstr, UI_MENU_BATTERY_FONT, BLACK, WHITE);

    return ret;
}

esp_err_t render_splash_to_buffer(const char *status_msg) {
    esp_err_t ret = ESP_OK;

    /* Clear buffer (White background) */
    Paint_Clear(WHITE);

    /* ------------------------------------------------------------------ */
    /* HEADER: Date (Left) and Battery (Right) | Y: 0 to 24               */
    /* ------------------------------------------------------------------ */
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    /* 1. Date in Top-Left Corner */
    char date_str[16];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
    Paint_DrawString_EN(8, 8, date_str, UI_MENU_DATE_FONT, BLACK, WHITE);

    /* 2. Battery in Top-Right Corner */
    int battery_pct = 0;
    ret = battery_update_cache(&battery_pct);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed after calling battery_update_cache() : %s", esp_err_to_name(ret));
    }
    if (battery_get_percentage(&battery_pct) == ESP_OK) {
        render_draw_battery_icon_top_right(battery_pct);
    } else {
	/* Fallback default value or warning if reading fails */
        render_draw_battery_icon_top_right(0); 
    }

    /* Horizontal header separator line */
    Paint_DrawLine(1, 24, 199, 24, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    /* ------------------------------------------------------------------ */
    /* MAIN BLOCK: Centered Time | Y: 25 to 100                           */
    /* ------------------------------------------------------------------ */
    char time_str[16];
    char tz_code[8];
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
    strftime(tz_code, sizeof(tz_code), "%Z", &timeinfo);

    ESP_LOGD(TAG, "Epoch: %lld | Local Time: %02d:%02d | TZ env: %s",
             (long long)now, timeinfo.tm_hour, timeinfo.tm_min, getenv("TZ") ? getenv("TZ") : "Not set");

    /* Large time in Font24 (~85px wide, centered from X=57) */
    Paint_DrawString_EN(57, 40, time_str, UI_MENU_TIME_FONT, BLACK, WHITE);

    /* Timezone indicator / secondary label */
    Paint_DrawString_EN(86, 70, tz_code, UI_MENU_TIMEZONE_FONT, BLACK, WHITE);

    /* Middle separator line */
    Paint_DrawLine(1, 100, 199, 100, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    /* ------------------------------------------------------------------ */
    /* FOOTER: System Uptime | Y: 101 to 200                              */
    /* ------------------------------------------------------------------ */
    Paint_DrawString_EN(12, 112, UI_MENU_SYS_UPTIME_STR, UI_MENU_SYS_UPTIME_T_FONT, BLACK, WHITE);

    /* Persistent accumulated uptime */
    char uptime_str[32];
    if (uptime_get_formatted(uptime_str, sizeof(uptime_str)) == ESP_OK) {
        Paint_DrawString_EN(12, 132, uptime_str, UI_MENU_SYS_UPTIME_FONT, BLACK, WHITE);
    }

    /* Battery diagnostic info */
    //char battery_debug_str[48];
    //snprintf(battery_debug_str, sizeof(battery_debug_str), "Bat mV: %lu | Raw: %d",
    //         (unsigned long)battery_get_last_voltage_mv(),
    //         battery_get_last_raw_avg());
    //Paint_DrawString_EN(12, 152, battery_debug_str, UI_MENU_DATE_FONT, BLACK, WHITE);

    /* System general status */
    Paint_DrawString_EN(12, 172, UI_MENU_STATUS_T_STR, UI_MENU_STATUS_FONT, BLACK, WHITE);
    if (status_msg)
        Paint_DrawString_EN(68, 172, status_msg, UI_MENU_STATUS_FONT, BLACK, WHITE);

    return ret;
}

esp_err_t render_voice_screen_to_buffer(const char *status_msg, const char *server_output_msg) {
    esp_err_t ret = ESP_OK;

    /* Clear buffer (White background) */
    Paint_Clear(WHITE);

    /* ------------------------------------------------------------------ */
    /* 1. HEADER: System Status | Y: 0 to 24                              */
    /* ------------------------------------------------------------------ */
    Paint_DrawString_EN(8, 8, UI_REC_STATUS_T_STR, UI_REC_STATUS_FONT, BLACK, WHITE);
    if (status_msg) {
        Paint_DrawString_EN(60, 8, (char *)status_msg, UI_REC_STATUS_FONT, BLACK, WHITE);
    }

    /* Horizontal header separator line */
    Paint_DrawLine(1, 24, 199, 24, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    /* ------------------------------------------------------------------ */
    /* 2. MAIN BLOCK: Result / Server Output | Y: 25 to 170               */
    /* ------------------------------------------------------------------ */
    if (server_output_msg != NULL && strlen(server_output_msg) > 0) {
        /* Render centered/adjusted message or command output in the body */
        Paint_DrawString_EN(12, 35, (char *)server_output_msg, UI_REC_SVR_OUT_FONT, BLACK, WHITE);
    } else {
        Paint_DrawString_EN(12, 35, "", UI_REC_SVR_OUT_FONT, BLACK, WHITE);
    }

    /* Bottom separator line */
    Paint_DrawLine(1, 168, 199, 168, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    /* ------------------------------------------------------------------ */
    /* 3. FOOTER: Button Guide (Corners) | Y: 176 to 200                  */
    /* ------------------------------------------------------------------ */
    /* Upper row: 2-Click for Record/Stop */
    Paint_DrawString_EN(8, 172, UI_REC_2_CLK_STR, UI_REC_CLICK_MSG_FONT, BLACK, WHITE);

    /* Lower row: 1-Click to Exit */
    Paint_DrawString_EN(8, 186, UI_REC_1_CLK_STR, UI_REC_CLICK_MSG_FONT, BLACK, WHITE);

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

    /* Get formatting function */
    const char* (*get_label)(int) = get_menu_active_editor_get_label();

    if (get_label != NULL) {
	/* For list-based elements / lookup tables (Timezone, Sleep Timeout) */
        snprintf(val_str, sizeof(val_str), "%s", get_label(curr_val));
    } else if (unit_suffix != NULL && strlen(unit_suffix) > 0) {
	/* For numeric values with suffix (e.g., "80 %") */
        snprintf(val_str, sizeof(val_str), "%d %s", curr_val, unit_suffix);
    } else {
	/* For simple integers without suffix (e.g., "10") */
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

    ret = epd_display_update_partial();

    return ret;
}

esp_err_t render_partial(void) {
    esp_err_t ret = ESP_OK; 

    ret = render(NULL, NULL);
    if (ret != ESP_OK)
        return ret;

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
