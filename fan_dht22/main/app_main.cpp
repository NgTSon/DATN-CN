/* Includes */
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <app_priv.h>
#include <app_reset.h>
#include <bsp/esp-bsp.h>
#include <common_macros.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>

/* Constants */
static const char *TAG = "app_main";
uint16_t fan_endpoint_id = 0;
uint16_t temperature_sensor_endpoint_id = 0;
uint16_t humidity_sensor_endpoint_id = 0;
constexpr auto k_timeout_seconds = 300;

/* Namespaces */
using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

#if CONFIG_ENABLE_ENCRYPTED_OTA
extern const char decryption_key_start[] asm("_binary_esp_image_encryption_key_pem_start");
extern const char decryption_key_end[] asm("_binary_esp_image_encryption_key_pem_end");

static const char *s_decryption_key = decryption_key_start;
static const uint16_t s_decryption_key_len = decryption_key_end - decryption_key_start;
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

/**
 * Handling Matter events
 */
static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type)
    {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
    {
        ESP_LOGI(TAG, "Fabric removed successfully");
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
        {
            chip::CommissioningWindowManager &commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
            constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
            if (!commissionMgr.IsCommissioningWindowOpen())
            {
                /* After removing last fabric, this example does not remove the Wi-Fi credentials
                 * and still has IP connectivity so, only advertising on DNS-SD.
                 */
                CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds,
                                                                            chip::CommissioningWindowAdvertisement::kDnssdOnly);
                if (err != CHIP_NO_ERROR)
                {
                    ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                }
            }
        }
        break;
    }

    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
        ESP_LOGI(TAG, "Fabric will be removed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
        ESP_LOGI(TAG, "Fabric is updated");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Fabric is committed");
        break;
    default:
        break;
    }
}

// Factoryreset
static esp_err_t factory_reset_button_register()
{
    button_handle_t push_button;
    esp_err_t err = bsp_iot_button_create(&push_button, NULL, BSP_BUTTON_NUM); // BSP_BUTTON_NUM = GPIO_NUM_9
    VerifyOrReturnError(err == ESP_OK, err);
    return app_reset_button_register(push_button);
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;
    // If user want to use driver, can be called from here.
    if (type == PRE_UPDATE) {
        /* Driver update */
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }
    return err;
}

/**
 * Main function that initializes Matter
 */
extern "C" void app_main()
{
    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    /* Initialize push button on the dev-kit to reset the device */
    esp_err_t err = factory_reset_button_register();
    ABORT_APP_ON_FAILURE(ESP_OK == err, ESP_LOGE(TAG, "Failed to initialize reset button, err:%d", err));

    /* Initialize driver */
    app_driver_handle_t temperature_sensor_handle = app_driver_DHT_sensor_init();
    // app_driver_handle_t humidity_sensor_handle = app_driver_DHT_sensor_init();

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

    if (node)
    {
        // Create endpoint with temperature measurement
        temperature_sensor::config_t temperature_sensor_config;
        temperature_sensor_config.temperature_measurement.measured_value = DEFAULT_TEMPERATURE_VALUE;
        endpoint_t *temperature_sensor_endpoint = temperature_sensor::create(node, &temperature_sensor_config, ENDPOINT_FLAG_NONE, temperature_sensor_handle);

        if (temperature_sensor_endpoint)
        {
            ESP_LOGI(TAG, "Created temperature endpoint");

            temperature_sensor_endpoint_id = endpoint::get_id(temperature_sensor_endpoint);
            ESP_LOGI(TAG, "Temperature endpoint created with endpoint_id %d", temperature_sensor_endpoint_id);
        }

        // Create endpoint with humidity measurement
        humidity_sensor::config_t humidity_sensor_config;
        humidity_sensor_config.relative_humidity_measurement.measured_value = DEFAULT_HUMIDITY_VALUE;
        endpoint_t *humidity_sensor_endpoint = humidity_sensor::create(node, &humidity_sensor_config, ENDPOINT_FLAG_NONE, NULL);

        if (humidity_sensor_endpoint)
        {
            ESP_LOGI(TAG, "Created humidity endpoint");

            humidity_sensor_endpoint_id = endpoint::get_id(humidity_sensor_endpoint);
            ESP_LOGI(TAG, "Humidity endpoint created with endpoint_id %d", humidity_sensor_endpoint_id);
        }
        
        // Create endpoint with FAN 
        fan::config_t fan_config;
        endpoint_t *fan_endpoint = fan::create(node, &fan_config, ENDPOINT_FLAG_NONE, NULL);
        // Create Cluster
        cluster_t *cluster = cluster::get(fan_endpoint, chip::app::Clusters::FanControl::Id);
        cluster::fan_control::feature::multi_speed::config_t multispeed_config;
        multispeed_config.speed_max = 100; 
        //multispeed_config.speed_current = 0;
        cluster::fan_control::feature::multi_speed::add(cluster, &multispeed_config);
        if(fan_endpoint){
            ESP_LOGI(TAG, "Create fan endpoint");

            fan_endpoint_id = endpoint::get_id(fan_endpoint);
            ESP_LOGI(TAG, "Fan endpoint created with endpoint_id %d", fan_endpoint_id);
        }

        /* These node and endpoint handles can be used to create/add other endpoints and clusters. */
        if (!node || !temperature_sensor_endpoint || !humidity_sensor_endpoint || fan_endpoint)
        {
            ESP_LOGE(TAG, "Matter node creation failed");
        }
    }

    /* Initialize app driver, according to the device_type.
    For now it's just for fan_control driver, Waiting for refinement*/
    app_driver_init();

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Matter start failed: %d", err);
    }

#if CONFIG_ENABLE_ENCRYPTED_OTA
    err = esp_matter_ota_requestor_encrypted_init(s_decryption_key, s_decryption_key_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialized the encrypted OTA, err: %d", err);
    }
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::wifi_register_commands();
    esp_matter::console::init();
#endif
}
