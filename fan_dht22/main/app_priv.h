
#pragma once

#include <esp_err.h>
#include <esp_matter.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

// FAN
#define LOW_MODE_PERCENT_MIN 1
#define LOW_MODE_PERCENT_MAX 33
#define MED_MODE_PERCENT_MIN 34
#define MED_MODE_PERCENT_MAX 66
#define HIGH_MODE_PERCENT_MIN 67
#define HIGH_MODE_PERCENT_MAX 100

// DHT22 Values
#define DHT22_GPIO_PIN GPIO_NUM_25
#define DEFAULT_TEMPERATURE_VALUE 1000
#define DEFAULT_HUMIDITY_VALUE 1000

#define DEFAULT_MEASURE_INTERVAL 3200 // 3,2s read DHT22

typedef void *app_driver_handle_t;
typedef void *fan_driver_handle_t;

/** Driver Update
 *
 * This API should be called to update the driver for the attribute being updated.
 * This is usually called from the common `app_attribute_update_cb()`.
 *
 * @param[in] driver_handle Handle to the driver.
 * @param[in] endpoint_id Endpoint ID of the attribute.
 * @param[in] cluster_id Cluster ID of the attribute.
 * @param[in] attribute_id Attribute ID of the attribute.
 * @param[in] val Pointer to `esp_matter_attr_val_t`. Use appropriate elements as per the value type.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val);

/** Initialize the temperature and humidity drivers
 *
 * This initializes the temperature and humidity drivers.
 *
 * @return Handle on success.
 * @return NULL in case of failure.
 */
// app_driver_handle_t app_driver_light_init();
app_driver_handle_t app_driver_DHT_sensor_init();


/** Set defaults for temperature and humidity drivers
 *
 * Set the attribute drivers to their default values from the created data model.
 *
 * @param[in] endpoint_id Endpoint ID of the driver.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
// esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id);
int16_t app_driver_read_temperature(uint16_t endpoint_id);
uint16_t app_driver_read_humidity(uint16_t endpoint_id);

/** Driver Initialize
 *
 * This API should be called to init driver(motor)
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_driver_init();

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG() \
    {                                         \
        .radio_mode = RADIO_MODE_NATIVE,      \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()               \
    {                                                      \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
#endif
