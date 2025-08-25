#include "drivers/fan_pwm.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "fan_pwm";

// LEDC timer configuration
static ledc_timer_config_t ledc_timer = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num       = LEDC_TIMER_0,
    .freq_hz         = 300,
    .clk_cfg         = LEDC_AUTO_CLK,
};

// LEDC channel configuration
static ledc_channel_config_t ledc_channel = {
    .gpio_num    = CONFIG_BLDC_PWM_GPIO, // Define this GPIO in Kconfig
    .speed_mode  = LEDC_LOW_SPEED_MODE,
    .channel     = LEDC_CHANNEL_0,
    .timer_sel   = LEDC_TIMER_0,
    .duty        = 0, // Start with 0 duty cycle
    .hpoint      = 0,
};

typedef struct {
    uint8_t is_start;   // Trạng thái bật/tắt quạt
    uint8_t target_speed; // Tốc độ mục tiêu của quạt
} motor_parameter_t;

static motor_parameter_t motor_parameter = {
    .is_start = 0,
    .target_speed = 0,
};

// Function init driver
esp_err_t fan_pwm_init() {
    esp_err_t ret;

    // Configure LEDC timer
    ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Fan driver initialized successfully");
    return ESP_OK;
}

esp_err_t fan_pwm_set_percent_speed(uint8_t percent) {
    if (percent > 100) {
        ESP_LOGE(TAG, "Invalid percent value: %d (must be 0-100)", percent);
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate duty cycle based on percentage
    uint32_t duty = (percent * ((1 << LEDC_TIMER_8_BIT) - 1)) / 100;
    esp_err_t ret = ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, duty);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty cycle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Apply the new duty cycle
    ret = ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty cycle: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Set PWM speed to %d%% (duty cycle: %" PRIu32 ")", percent, duty);

    return ESP_OK;
}

// Function mode controll of Fan
esp_err_t fan_pwm_set_fanmode(uint8_t status) {
    switch (status) {
        case 0: // OFF mode
            motor_parameter.is_start = 0;
            motor_parameter.target_speed = 0;
            ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
            ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
            ESP_LOGI(TAG, "Fan is turned OFF");
            break;
        case 1: // Low speed
            motor_parameter.is_start = 1;
            motor_parameter.target_speed = LOW_MODE_PERCENT;
            ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, LOW_MODE_PERCENT * 255 / 100);
            ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
            ESP_LOGI(TAG, "Fan is set to LOW speed (%d%%)", LOW_MODE_PERCENT);
            break;
        case 2: // Medium speed
            motor_parameter.is_start = 1;
            motor_parameter.target_speed = MED_MODE_PERCENT;
            ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, MED_MODE_PERCENT * 255 / 100);
            ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
            ESP_LOGI(TAG, "Fan is set to MEDIUM speed (%d%%)", MED_MODE_PERCENT);
            break;
        case 3: // High speed
            motor_parameter.is_start = 1;
            motor_parameter.target_speed = HIGH_MODE_PERCENT;
            ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, HIGH_MODE_PERCENT * 255 / 100);
            ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
            ESP_LOGI(TAG, "Fan is set to HIGH speed (%d%%)", HIGH_MODE_PERCENT);
            break;
        default:
            ESP_LOGE(TAG, "Invalid mode: %d", status);
            return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}
