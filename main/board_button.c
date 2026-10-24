#include "driver/gpio.h"
#include "board_button.h"

static const char *TAG = "BOARD_BUTTON";

esp_err_t button_hardware_init(gpio_num_t gpio_num) {
    esp_err_t ret = ESP_OK;

    /* Configure GPIO input parameters with internal pull-up enabled */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    /* Apply configuration to the physical GPIO hardware */
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button on GPIO %d: %s", (int)gpio_num, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Button hardware successfully initialized on GPIO %d", (int)gpio_num);
    return ret;
}
