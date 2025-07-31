
////////////////////////////// DHT22 CODE //////////////////////////////

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "drivers/DHT22X.h"

// DHT timer precision in microseconds
#define DHT_TIMER_INTERVAL 2
#define DHT_DATA_BITS 40
#define DHT_DATA_BYTES (DHT_DATA_BITS / 8)

// == global defines =============================================

static const char *TAG = "DHTX";

float humidity = 0.;
float temperature = 0.;

// == get temp & hum =============================================

/**
 * Get the humidity
 */
float getHumidity()
{
    return humidity;
}

/**
 * Get the temperature
 */
float getTemperature()
{
    return temperature;
}

// == error handler ===============================================

/**
 * Error handler
 * @param response
 */
void errorHandler(int response)
{
    switch (response)
    {

    case DHT_TIMEOUT_ERROR:
        printf("Sensor Timeout\n");
        break;

    case DHT_CHECKSUM_ERROR:
        printf("CheckSum error\n");
        break;

    case DHT_OK:
        break;

    default:
        printf("Unknown error\n");
    }
}

/**
 * Get the signal level
 * @param usTimeOut Timeout
 * @param state State of the signal
 * @return uSec is number of microseconds passed
 */

// == Get signal level with timeout ===============================
int getSignalLevel(int usTimeOut, bool state)
{
    int64_t start = esp_timer_get_time(); // Lấy thời gian bắt đầu
    while (gpio_get_level(DHT_GPIO) == state)
    {
        // Nếu vượt quá thời gian timeout, trả về -1
        if ((esp_timer_get_time() - start) > usTimeOut)
        {
            return -1;
        }

        // Tăng thêm thời gian trễ nhỏ để giảm tải CPU và tránh jitter
        esp_rom_delay_us(1);
    }

    // Trả về thời gian đã trôi qua
    return (int)(esp_timer_get_time() - start);
}



///////////////////////////////////////////////
/**
 * Pack two data bytes into single value and take into account sign bit.
 */

static inline int16_t dht_convert_data(uint8_t msb, uint8_t lsb)
{
    int16_t data;
    data = msb & 0x7F;
    data <<= 8;
    data |= lsb;

    if (msb & BIT(7))
        data = -data; // convert it to negative

    return data;
}


static inline esp_err_t dht_fetch_data(uint8_t data[DHT_DATA_BYTES])
{
    int uSec = 0;
    uint8_t byteInx = 0;
    uint8_t bitInx = 7;

    for (int k = 0; k < DHT_DATA_BYTES; k++)
        data[k] = 0;

    gpio_set_direction(DHT_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_GPIO, 0);
    esp_rom_delay_us(3000);

    gpio_set_level(DHT_GPIO, 1);
    esp_rom_delay_us(25);

    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT);

    // Step through Phase 'B', 40us
    uSec = getSignalLevel(85, 0);
    if (uSec < 0)
        return ESP_FAIL;

    uSec = getSignalLevel(85, 1);
    if (uSec < 0)
        return ESP_FAIL;

    for (int k = 0; k < DHT_DATA_BITS; k++)
    {
        uSec = getSignalLevel(56, 0);
        if (uSec < 0)
            return ESP_FAIL;

        uSec = getSignalLevel(75, 1);
        if (uSec < 0)
            return ESP_FAIL;

        if (uSec > 40)
        {
            data[byteInx] |= (1 << bitInx);
        }

        if (bitInx == 0)
        {
            bitInx = 7;
            ++byteInx;
        }
        else
            bitInx--;
    }

    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
        return ESP_OK;
    else
        return ESP_FAIL;
}


esp_err_t dht_read_data()
{
    uint8_t data[DHT_DATA_BYTES] = {0};

    gpio_set_direction(DHT_GPIO, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(DHT_GPIO, 1);

    esp_err_t result = dht_fetch_data(data);

    if (result == ESP_OK)
    {
        // PORT_EXIT_CRITICAL();
    }

    /* restore GPIO direction because, after calling dht_fetch_data(), the
     * GPIO direction mode changes */
    gpio_set_direction(DHT_GPIO, GPIO_MODE_OUTPUT_OD);

    gpio_set_level(DHT_GPIO, 1);

    if (result != ESP_OK)
    {
        printf("==== dht_read_data - ESP_NOT_OK ====\n");
        return result;
    }

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
    {
        ESP_LOGE(TAG, "Checksum failed, invalid data received from sensor");

        return ESP_ERR_INVALID_CRC;
    }

    humidity = dht_convert_data(data[0], data[1]) / 10;
    temperature = dht_convert_data(data[2], data[3]) / 10;

    ESP_LOGI(TAG, "Sensor data: humidity=%f, temp=%f", humidity, temperature);

    return ESP_OK;
}

esp_err_t dht_read_float_data()
{
    esp_err_t res = dht_read_data();

    if (res != ESP_OK)
    {
        return res;
    }

    return ESP_OK;
}

////////////////////////////// END OF DHT22 CODE //////////////////////////////
