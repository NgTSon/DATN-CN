/*
 * app_driver.cpp
 *
 *
 */

/* Includes ------------------------------------------------------------------*/
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <cstdint>
#include <cmath>
#include <nvs_flash.h>
#include <esp_system.h>

#include <app_priv.h>

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_matter.h>

#include "drivers/fan_pwm.h"
#include "drivers/DHT22X.h"


/* Constants -----------------------------------------------------------------*/
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::FanControl;
using namespace esp_matter::cluster;

static const char *TAG = "app_driver";
extern uint16_t temperature_sensor_endpoint_id;
extern uint16_t humidity_sensor_endpoint_id;
extern uint16_t fan_endpoint_id;

///////////////////////////        Driver DHT22 ///////////////////////////////////

/**
 * Update Matter values with temperature and humidity
 */
void updateMatterWithValues()
{
    // Update temperature values
    esp_matter_attr_val_t temperature_value;
    temperature_value = esp_matter_invalid(NULL);
    temperature_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_INT16;
    temperature_value.val.i16 = app_driver_read_temperature(temperature_sensor_endpoint_id);
    esp_matter::attribute::update(temperature_sensor_endpoint_id, TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &temperature_value);

    // Update humidity values
    esp_matter_attr_val_t humidity_value;
    humidity_value = esp_matter_invalid(NULL);
    humidity_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_UINT16;
    humidity_value.val.u16 = app_driver_read_humidity(humidity_sensor_endpoint_id);
    esp_matter::attribute::update(humidity_sensor_endpoint_id, RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &humidity_value);
}



int16_t app_driver_read_temperature(uint16_t endpoint_id)
{
    return (u_int16_t)getTemperature() * 100;
}

uint16_t app_driver_read_humidity(uint16_t endpoint_id)
{
    return (u_int16_t)getHumidity() * 100;
}


esp_err_t sensor_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    // if (type == POST_UPDATE)
    if (type == PRE_UPDATE)
    {
        if (cluster_id == TemperatureMeasurement::Id)
        {
            ESP_LOGI(TAG, "Temperature attribute updated to %d", val->val.i16);
        }
        else if (cluster_id == RelativeHumidityMeasurement::Id)
        {
            ESP_LOGI(TAG, "Humidity attribute updated to %d", val->val.i16);
        }
    }
    return ESP_OK;
}

/**
 * DHT22 Sensor task
 */
static void DHT22_task(void *pvParameter)
{
    // setDHTgpio(DHT_GPIO);
    ESP_LOGI(TAG, "Starting DHT task\n\n");

    for (;;)
    {
        ESP_LOGI(TAG, "=== Reading DHT ===\n");
        int ret = dht_read_float_data();

        errorHandler(ret);

        // Update Matter values
        updateMatterWithValues();

        // Wait at least 3 seconds before reading again
        // The interval of the whole process must be more than 3 seconds
        vTaskDelay(DEFAULT_MEASURE_INTERVAL / portTICK_PERIOD_MS);
    }
}

/**
 * Initialize DHT22 sensor
 */
app_driver_handle_t app_driver_DHT_sensor_init()
{
    // Start DHT22 sensor task
    // xTaskCreatePinnedToCore(&DHT22_task, "DHT22_task", DHT22_TASK_STACK_SIZE, NULL, DHT22_TASK_PRIORITY, NULL, DHT22_TASK_CORE_ID);
    xTaskCreatePinnedToCore(DHT22_task, "DHT22 Task", 4096, NULL, 4, NULL, 0);
    // Return a placeholder value (you may need to adapt the return type)
    return (app_driver_handle_t)1;
}


////////////////////// END Driver DHT22 ///////////////////////////////////



//////////// BEGIN DRIVER FAN //////////////////////////

// get and set attribute value FAN
static void get_attribute(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);
    attribute::get_val(attribute, val);
}

static esp_err_t set_attribute(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t val)
{
    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);
    return attribute::set_val(attribute, &val);
}

static esp_err_t app_driver_fan_set_percent(fan_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    ESP_LOGI(TAG, "Percent received value = %d", val->val.i);
    fan_pwm_set_percent_speed(val->val.i);
    return ESP_OK;
}

static esp_err_t app_driver_fan_set_mode(fan_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    ESP_LOGI(TAG, "Mode received value = %d ", val->val.i);
    fan_pwm_set_fanmode(val->val.i);
    return ESP_OK;
}

static bool check_if_mode_percent_match(uint8_t fan_mode, uint8_t percent)
{
    switch (fan_mode) {
        case chip::to_underlying(FanModeEnum::kHigh): {
            if (percent < HIGH_MODE_PERCENT_MIN) {
                return false;
            }
            break;
        }
        case chip::to_underlying(FanModeEnum::kMedium): {
            if ((percent < MED_MODE_PERCENT_MIN) || (percent > MED_MODE_PERCENT_MAX)) {
               return false;
            }
            break;
        }
        case chip::to_underlying(FanModeEnum::kLow): {
            if ((percent < LOW_MODE_PERCENT_MIN) || (percent > LOW_MODE_PERCENT_MAX)) {
                return false;
            }
            break;
        }
        default:
            return false;
    }

    return true;
}



esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;

    if (endpoint_id == fan_endpoint_id) {
       fan_driver_handle_t handle = (fan_driver_handle_t)driver_handle;

        if (cluster_id == FanControl::Id) {
            if (attribute_id == FanControl::Attributes::FanMode::Id) {
                esp_matter_attr_val_t val_a = esp_matter_invalid(NULL);
                get_attribute(endpoint_id, FanControl::Id, Attributes::PercentSetting::Id, &val_a);
                /* When FanMode attribute change , should check the persent setting value, if this value not match
                   FanMode, need write the persent setting value to corresponding value. Now we set it to the max
                   value of the FanMode */
                if (!check_if_mode_percent_match(val->val.u8, val_a.val.u8)) {
                    switch (val->val.u8) {
                        case chip::to_underlying(FanModeEnum::kHigh): {
                                val_a.val.u8 = HIGH_MODE_PERCENT_MAX;
                                set_attribute(endpoint_id, FanControl::Id, Attributes::PercentSetting::Id, val_a);
                                set_attribute(endpoint_id, FanControl::Id, Attributes::SpeedSetting::Id, val_a);

                            break;
                        }
                        case chip::to_underlying(FanModeEnum::kMedium): {
                                val_a.val.u8 = MED_MODE_PERCENT_MAX;
                                set_attribute(endpoint_id, FanControl::Id, Attributes::PercentSetting::Id, val_a);
                                set_attribute(endpoint_id, FanControl::Id, Attributes::SpeedSetting::Id, val_a);
                            break;
                        }
                        case chip::to_underlying(FanModeEnum::kLow): {
                                val_a.val.u8 = LOW_MODE_PERCENT_MAX;
                                set_attribute(endpoint_id, FanControl::Id, Attributes::PercentSetting::Id, val_a);
                                set_attribute(endpoint_id, FanControl::Id, Attributes::SpeedSetting::Id, val_a);
                            break;
                        }
                        case chip::to_underlying(FanModeEnum::kAuto): {
                            /* add auto mode driver for auto logic */
                            break;
                        }
                        case chip::to_underlying(FanModeEnum::kOff): {
                            /* add off mode driver if needed */
                            break;
                        }
                        default:
                            break;
                    }
                }
                set_attribute(endpoint_id, FanControl::Id, FanControl::Attributes::PercentCurrent::Id, val_a);
                set_attribute(endpoint_id, FanControl::Id, FanControl::Attributes::SpeedCurrent::Id, val_a);

                attribute::report(endpoint_id, FanControl::Id, FanControl::Attributes::PercentCurrent::Id,&val_a);
                attribute::report(endpoint_id, FanControl::Id, FanControl::Attributes::SpeedCurrent::Id,&val_a);

                err = app_driver_fan_set_mode(handle, val);
            } else if (attribute_id == FanControl::Attributes::PercentSetting::Id) {
                esp_matter_attr_val_t val_b = esp_matter_invalid(NULL);
                get_attribute(endpoint_id, FanControl::Id, Attributes::FanMode::Id, &val_b);
                if (!check_if_mode_percent_match(val_b.val.u8, val->val.u8)) {
                    if (val->val.u8 >= HIGH_MODE_PERCENT_MIN) {
                        /* set high mode */
                        val_b.val.u8 = chip::to_underlying(FanModeEnum::kHigh);
                        set_attribute(endpoint_id, FanControl::Id, Attributes::FanMode::Id, val_b);
                    } else if (val->val.u8 >= MED_MODE_PERCENT_MIN) {
                        /* set med mode */
                        val_b.val.u8 = chip::to_underlying(FanModeEnum::kMedium);
                        set_attribute(endpoint_id, FanControl::Id, Attributes::FanMode::Id, val_b);
                    } else if (val->val.u8 >= LOW_MODE_PERCENT_MIN) {
                        /* set low mode */
                        val_b.val.u8 = chip::to_underlying(FanModeEnum::kLow);
                        set_attribute(endpoint_id, FanControl::Id, Attributes::FanMode::Id, val_b);;
                    }
                }
                set_attribute(endpoint_id, FanControl::Id, FanControl::Attributes::PercentCurrent::Id, *val);
                set_attribute(endpoint_id, FanControl::Id, FanControl::Attributes::SpeedCurrent::Id, *val);
                
                attribute::report(endpoint_id, FanControl::Id, FanControl::Attributes::PercentCurrent::Id,val);
                attribute::report(endpoint_id, FanControl::Id, FanControl::Attributes::SpeedCurrent::Id,val);

                err = app_driver_fan_set_percent(handle, val);
            } 
        }
    }
    return err;
}

esp_err_t fan_driver_init()
{
    esp_err_t err = ESP_OK;
    /* initializing fan */
    err = fan_pwm_init();
    return err;
}

esp_err_t app_driver_init()
{
    esp_err_t err = ESP_OK;
    /* initializing fan driver */
    fan_driver_init();
    return err;
}


/////////// END DRIVER FAN ////////////////////////////////
