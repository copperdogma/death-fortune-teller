/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <math.h>
#include <driver/i2c.h> // Use legacy I2C driver
#include <cmath> // For NAN
#include <inttypes.h> // For PRIu32

#include <lib/support/CodeUtils.h>
#include <esp_matter.h> // For chip::app::Clusters and esp_matter_invalid
#include <app/ConcreteAttributePath.h> // For chip::app::Clusters definitions

#include "shtc3.h"

#define I2C_MASTER_SCL_IO           CONFIG_SHTC3_I2C_SCL_PIN      /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           CONFIG_SHTC3_I2C_SDA_PIN      /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                     /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          400000                        /*!< I2C master clock frequency */
#define SHTC3_SENSOR_ADDR           0x70                          /*!< slave address for SHTC3 sensor */

// SHTC3 Commands (Needed for shtc3_sensor_init internal operations)
#define SHTC3_WAKE_UP_COMMAND_MSB           0x35
#define SHTC3_WAKE_UP_COMMAND_LSB           0x17
#define SHTC3_SLEEP_COMMAND_MSB             0xB0
#define SHTC3_SLEEP_COMMAND_LSB             0x98
#define SHTC3_READ_ID_COMMAND_MSB           0xEF
#define SHTC3_READ_ID_COMMAND_LSB           0xC8
#define SHTC3_MEASURE_T_FIRST_MSB           0x7C // Low power, T first, Clock stretching enabled
#define SHTC3_MEASURE_T_FIRST_LSB           0xA2 // Low power, T first, Clock stretching enabled

static const char *TAG = "shtc3_driver";

#define SHTC3_PRODUCT_CODE_MASK             0x083F // From datasheet, bits 5 and 11-15 are don't care
#define SHTC3_PRODUCT_CODE_SHTC3            0x0807 // SHTC3 product code is 0b0000_1000_0xxx_0111

#define SHTC3_PRODUCT_CODE_SIZE_BYTES       2

// Global static variable to store the sensor configuration
static shtc3_sensor_config_t *g_sensor_config = NULL;
// Global static variable for the timer handle
static TimerHandle_t g_sensor_timer_handle = NULL;
static bool g_is_sensor_initialized = false;

static esp_err_t shtc3_read(uint8_t *data, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHTC3_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true /* enable_ack */);
    // Read temperature first then humidity, with clock stretching enabled
    // SHTC3 specific command: Wakeup (0x3517) then Read Temp/RH High Precision with Clock Stretching (0x7CA2)
    // However, simpler approach is to send measurement command, wait, then read.
    // Command: Measure T & RH, clock stretching, high precision (0x7CA2)
    i2c_master_write_byte(cmd, 0x7C, true /* enable_ack */); // SHTC3_CMD_MEASURE_HPM
    i2c_master_write_byte(cmd, 0xA2, true /* enable_ack */); // 
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SHTC3: Failed to send measurement command: %s", esp_err_to_name(err));
        return err;
    }
    cmd = NULL;

    // Wait for measurement to complete. SHTC3 datasheet specifies max 12.1 ms for HPM.
    vTaskDelay(pdMS_TO_TICKS(15)); 

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHTC3_SENSOR_ADDR << 1) | I2C_MASTER_READ, true /* enable_ack */);
    // Read 6 bytes: Temp_MSB, Temp_LSB, Temp_CRC, RH_MSB, RH_LSB, RH_CRC
    err = i2c_master_read(cmd, data, size, I2C_MASTER_LAST_NACK);
     if (err != ESP_OK) { // Check error after read setup
        ESP_LOGE(TAG, "SHTC3: Failed to setup read: %s", esp_err_to_name(err));
        i2c_cmd_link_delete(cmd);
        return err;
    }
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive data from SHTC3, err:%d (%s)", err, esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

// SHTC3 CRC checksum function
static uint8_t shtc3_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int j = 0; j < len; j++) {
        crc ^= data[j];
        for (int i = 0; i < 8; i++) {
            if ((crc & 0x80) != 0) {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}


static void shtc3_sensor_report_task(void *pvParameters)
{
    if (!g_sensor_config) {
        ESP_LOGE(TAG, "Sensor not configured, cannot report.");
        vTaskDelete(NULL);
        return;
    }

    uint8_t data_rd[6]; // Temp MSB, LSB, CRC, RH MSB, LSB, CRC
    esp_err_t err = shtc3_read(data_rd, sizeof(data_rd));

    if (err == ESP_OK) {
        uint16_t temp_raw = (data_rd[0] << 8) | data_rd[1];
        uint8_t temp_crc = data_rd[2];
        uint16_t humidity_raw = (data_rd[3] << 8) | data_rd[4];
        uint8_t humidity_crc = data_rd[5];

        if (shtc3_crc8(data_rd, 2) != temp_crc) {
            ESP_LOGE(TAG, "Temperature CRC check failed");
        } else {
            float temperature = -45.0f + 175.0f * (temp_raw / 65535.0f);
            ESP_LOGI(TAG, "Temperature: %.2f C", temperature);
            if (g_sensor_config->temperature.cb) {
                g_sensor_config->temperature.cb(g_sensor_config->temperature.endpoint_id, temperature, g_sensor_config->user_data);
            }
        }

        if (shtc3_crc8(data_rd + 3, 2) != humidity_crc) {
            ESP_LOGE(TAG, "Humidity CRC check failed");
        } else {
            float humidity = 100.0f * (humidity_raw / 65535.0f);
            // Ensure humidity is within 0-100%
            humidity = (humidity < 0.0f) ? 0.0f : humidity;
            humidity = (humidity > 100.0f) ? 100.0f : humidity;
            ESP_LOGI(TAG, "Humidity: %.2f %%", humidity);
            if (g_sensor_config->humidity.cb) {
                g_sensor_config->humidity.cb(g_sensor_config->humidity.endpoint_id, humidity, g_sensor_config->user_data);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to read from SHTC3 sensor");
        // Optionally, report a default/error value or NaN
        if (g_sensor_config->temperature.cb) {
             g_sensor_config->temperature.cb(g_sensor_config->temperature.endpoint_id, NAN, g_sensor_config->user_data);
        }
        if (g_sensor_config->humidity.cb) {
            g_sensor_config->humidity.cb(g_sensor_config->humidity.endpoint_id, NAN, g_sensor_config->user_data);
        }
    }
    vTaskDelete(NULL);
}


static void shtc3_sensor_timer_cb(TimerHandle_t xTimer)
{
    // Create a task to handle sensor reading and reporting
    // This avoids blocking the timer callback, which should be short
    xTaskCreate(shtc3_sensor_report_task, "shtc3_report", 2048, NULL, 5, NULL);
}

esp_err_t shtc3_sensor_init(shtc3_sensor_config_t *config_param)
{
    ESP_LOGI(TAG, "Initializing SHTC3 sensor");
    if (g_is_sensor_initialized) {
        ESP_LOGI(TAG, "SHTC3 sensor already initialized");
        return ESP_OK;
    }

    if (!config_param) {
        ESP_LOGE(TAG, "SHTC3 config cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!config_param->temperature.cb && !config_param->humidity.cb) {
        ESP_LOGE(TAG, "At least one callback (temperature or humidity) must be provided");
        return ESP_ERR_INVALID_ARG;
    }

    g_sensor_config = config_param; // Store the provided config

    // Initialize I2C master
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = I2C_MASTER_FREQ_HZ},
        .clk_flags = 0, // Explicitly initialize clk_flags
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C master config failed: %s", esp_err_to_name(err));
        g_sensor_config = NULL;
        return err;
    }
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        g_sensor_config = NULL;
        return err;
    }

    // Verify sensor presence and product code (simplified from shtc3_init)
    // Wake up sensor
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHTC3_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SHTC3_WAKE_UP_COMMAND_MSB, true);
    i2c_master_write_byte(cmd, SHTC3_WAKE_UP_COMMAND_LSB, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake up SHTC3 during init: %s", esp_err_to_name(err));
        i2c_driver_delete(I2C_MASTER_NUM);
        g_sensor_config = NULL;
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Short delay after wakeup

    // Read ID
    uint8_t id_data[SHTC3_PRODUCT_CODE_SIZE_BYTES];
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHTC3_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SHTC3_READ_ID_COMMAND_MSB, true);
    i2c_master_write_byte(cmd, SHTC3_READ_ID_COMMAND_LSB, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
     if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send read ID command SHTC3: %s", esp_err_to_name(err));
        i2c_driver_delete(I2C_MASTER_NUM);
        g_sensor_config = NULL;
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Short delay

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHTC3_SENSOR_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, id_data, SHTC3_PRODUCT_CODE_SIZE_BYTES, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read SHTC3 product code: %s", esp_err_to_name(err));
        i2c_driver_delete(I2C_MASTER_NUM);
        g_sensor_config = NULL;
        return err;
    }

    uint16_t product_code = (id_data[0] << 8) | id_data[1];
    if ((product_code & SHTC3_PRODUCT_CODE_MASK) != SHTC3_PRODUCT_CODE_SHTC3) {
        ESP_LOGE(TAG, "SHTC3 product code mismatch. Expected: 0x%04X, Got: 0x%04X", SHTC3_PRODUCT_CODE_SHTC3, product_code);
        i2c_driver_delete(I2C_MASTER_NUM);
        g_sensor_config = NULL;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SHTC3 Product code: 0x%04X", product_code);

    // Put sensor to sleep
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHTC3_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SHTC3_SLEEP_COMMAND_MSB, true);
    i2c_master_write_byte(cmd, SHTC3_SLEEP_COMMAND_LSB, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to put SHTC3 to sleep during init: %s", esp_err_to_name(err));
        // Not a fatal error for init, sensor might just consume more power
    }


    // Create timer for periodic reading
    g_sensor_timer_handle = xTimerCreate("shtc3_timer", pdMS_TO_TICKS(g_sensor_config->interval_ms),
                                       true /* auto-reload */, NULL /* timer ID */, shtc3_sensor_timer_cb);
    if (g_sensor_timer_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create SHTC3 timer");
        i2c_driver_delete(I2C_MASTER_NUM);
        g_sensor_config = NULL;
        return ESP_FAIL;
    }

    if (xTimerStart(g_sensor_timer_handle, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start SHTC3 timer");
        xTimerDelete(g_sensor_timer_handle, 0);
        g_sensor_timer_handle = NULL;
        i2c_driver_delete(I2C_MASTER_NUM);
        g_sensor_config = NULL;
        return ESP_FAIL;
    }

    g_is_sensor_initialized = true;
    ESP_LOGI(TAG, "SHTC3 sensor initialized successfully, polling every %" PRIu32 " ms", g_sensor_config->interval_ms);
    return ESP_OK;
}
