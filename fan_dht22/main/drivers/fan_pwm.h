#pragma once
#include <stdint.h>
#include "esp_err.h"

// Các giá trị phần trăm tương ứng với chế độ quạt
#define LOW_MODE_PERCENT     33
#define MED_MODE_PERCENT     66
#define HIGH_MODE_PERCENT    100

// Initialize the FAN driver
esp_err_t fan_pwm_init();

// Set the speed of the FAN as a percentage (0-100%)
esp_err_t fan_pwm_set_percent_speed(uint8_t percent);

// set mode of the FAN
esp_err_t fan_pwm_set_fanmode(uint8_t status);