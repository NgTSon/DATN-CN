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

#include <math.h>

#include "drivers/bh1750.h"
#include "drivers/i2cdev.h"
#include "drivers/pir.h"

/* Constants -----------------------------------------------------------------*/
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;

static const char *TAG = "app_driver";
extern uint16_t light_sensor_endpoint_id;
extern uint16_t occupancy_sensor_endpoint_id;

static volatile int16_t light_sensor_lux = -1; // Giá trị ánh sáng (khởi tạo -1 để kiểm tra trạng thái chưa đọc được)

#if defined(CONFIG_EXAMPLE_I2C_ADDRESS_LO)
#define ADDR BH1750_ADDR_LO
#endif
#if defined(CONFIG_EXAMPLE_I2C_ADDRESS_HI)
#define ADDR BH1750_ADDR_HI
#endif

/////////////       BH1750 /////////////////
/**
 * Update Matter values with illuminance
 */
void updateMatterWithValues()
{
    // Update temperature values
    esp_matter_attr_val_t illuminance_value;
    illuminance_value = esp_matter_invalid(NULL);
    illuminance_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_INT16;
    illuminance_value.val.i16 = app_driver_read_light_sensor(light_sensor_endpoint_id);
    esp_matter::attribute::update(light_sensor_endpoint_id, IlluminanceMeasurement::Id, IlluminanceMeasurement::Attributes::MeasuredValue::Id, &illuminance_value);
}

/**
 * Functions read lightsensor
 */
int16_t app_driver_read_light_sensor(uint16_t endpoint_id)
{
    // Return value present
    return light_sensor_lux;
}

// Update
esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;

    /* Do stuff here */

    return err;
}

/**
 * BH1759 Sensor task
 */

static void BH1750_task(void *pvParameter)
{
    // Khởi tạo cấu hình I2C
    i2c_config_t i2c_config;
    i2c_config.mode = I2C_MODE_MASTER;
    i2c_config.sda_io_num = (gpio_num_t)CONFIG_EXAMPLE_I2C_MASTER_SDA;
    i2c_config.scl_io_num = (gpio_num_t)CONFIG_EXAMPLE_I2C_MASTER_SCL;
    i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_config.master.clk_speed = 400000; // 400kHz
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0)); // Cài đặt driver

    // Khởi tạo mô tả BH1750
    ESP_ERROR_CHECK(i2cdev_init()); // Init library
    i2c_dev_t dev;
    memset(&dev, 0, sizeof(i2c_dev_t)); // Zero descriptor
    ESP_ERROR_CHECK(bh1750_init_desc(&dev, ADDR, I2C_NUM_0,
                                     (gpio_num_t)CONFIG_EXAMPLE_I2C_MASTER_SDA,
                                     (gpio_num_t)CONFIG_EXAMPLE_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH));

    while (1)
    {
        uint16_t lux;

        if (bh1750_read(&dev, &lux) != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not read lux data");
        }
        else
        {
            ESP_LOGI(TAG, "Lux: %d", lux);
        }

        light_sensor_lux = (int16_t)(10000 * log10(lux)); // Convert value lux by standard Matter

        // Update Matter values
        updateMatterWithValues();

        // Wait at seconds before reading again
        vTaskDelay(DEFAULT_MEASURE_INTERVAL / portTICK_PERIOD_MS);
    }
}


/**
 * Initialize BH1750 sensor
 */
app_driver_handle_t app_driver_BH_sensor_init()
{
    // Start BH1750 sensor task
    xTaskCreatePinnedToCore(BH1750_task, "BH1750 Task", 4096, NULL, 4, NULL, 0);
    // Return a placeholder value (you may need to adapt the return type)
    return (app_driver_handle_t)1;
}

/////////////////////////////////// PIR ///////////////////////
void occupancy_sensor_notification(uint16_t endpoint_id, bool occupancy, void *user_data)
{
    // schedule the attribute update so that we can report it from matter thread
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, occupancy]() {
        attribute_t * attribute = attribute::get(endpoint_id,
                                                 OccupancySensing::Id,
                                                 OccupancySensing::Attributes::Occupancy::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.b = occupancy;

        attribute::update(endpoint_id, OccupancySensing::Id, OccupancySensing::Attributes::Occupancy::Id, &val);
    });
}

