#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <nvs_flash.h>

#include <esp_matter.h>

#include <app_priv.h>

#include <driver/ledc.h>
#include <esp_err.h>
#include <freertos/timers.h>


static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;
using namespace chip::app::Clusters;
using namespace esp_matter;

#define LED_PIN         ((gpio_num_t) 5)       // Chân GPIO điều khiển LED
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_FREQUENCY  5000    // Tần số PWM (Hz)

// Status On/Off
static bool is_led_on = false;

// Function init PWM
static void app_driver_pwm_init()
{
    // Cấu hình timer cho LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Cấu hình kênh LEDC
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = LED_PIN,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0, // Mặc định độ sáng = 0
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// Function controll On/Off LED
static esp_err_t app_driver_light_set_on_off(esp_matter_attr_val_t *val)
{
    ESP_LOGI(TAG, "Set LED On/Off: %d", val->val.b);

    is_led_on = val->val.b;
    if (is_led_on) {
        // Bật đèn với độ sáng hiện tại
        uint32_t duty = ledc_get_duty(LEDC_MODE, LEDC_CHANNEL);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    } else {
        // Tắt đèn
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }

    return ESP_OK;
}

// Function controll light = PWM
static esp_err_t app_driver_light_set_pwm(esp_matter_attr_val_t *val)
{
    ESP_LOGI(TAG, "Set PWM Level: %d", val->val.u8);

    if (is_led_on) {
        uint32_t duty = (val->val.u8 * 8191) / 255; // Map độ sáng (0-255) thành (0-2^13-1)
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }

    return ESP_OK;
}

// Function update attribute
esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;

    if (endpoint_id == light_endpoint_id) {
        if (cluster_id == OnOff::Id) { // Bật/Tắt LED
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_on_off(val);
            }
        } else if (cluster_id == LevelControl::Id) { // Điều chỉnh độ sáng LED
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_pwm(val);
            }
        }
    }

    return err;
}

// Function init driver LED
app_driver_handle_t app_driver_light_init()
{
    ESP_LOGI(TAG, "Initializing LED driver...");
    app_driver_pwm_init();
    return nullptr;
}

// Function set value default
esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    // Set on Default
    cluster = cluster::get(endpoint, OnOff::Id);
    attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_on_off(&val);

    // Set on level Default
    cluster = cluster::get(endpoint, LevelControl::Id);
    attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_pwm(&val);

    return err;
}
