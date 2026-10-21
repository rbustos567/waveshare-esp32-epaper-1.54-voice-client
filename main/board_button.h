#ifndef BOARD_BUTTON_H
#define BOARD_BUTTON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t button_hardware_init(gpio_num_t gpio_num);

#ifdef __cplusplus
}
#endif

#endif // BOARD_BUTTON_H

