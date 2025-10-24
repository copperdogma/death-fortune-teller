#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pir_sensor.h"

// ESP IDF timer includes
#include "esp_timer.h"

// Forward declaration for the C-callable function in app_main.cpp
extern uint16_t get_pir_unoccupied_delay_seconds(uint16_t endpoint_id);

static const char *TAG = "pir_sensor";

// Queue to send GPIO events from ISR to a task
static QueueHandle_t gpio_evt_queue = NULL;

// Occupancy state - true if occupied, false if not.
// This should be updated and used to drive Matter cluster updates.
static volatile bool g_occupancy_state = false; 
static pir_sensor_config_t g_pir_config;
static esp_timer_handle_t g_unoccupied_timer = NULL;

// Forward declaration of the ISR handler
static void IRAM_ATTR gpio_isr_handler(void* arg);
// Forward declaration of the task that will process PIR events
static void pir_sensor_task(void* arg);
// Forward declaration for timer callback
static void unoccupied_timer_callback(void* arg);

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    // For now, we just send the event to the queue. 
    // More sophisticated logic might be needed if debouncing becomes an issue,
    // or if we want to distinguish RISING/FALLING edges directly in ISR (though generally better in task).
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void unoccupied_timer_callback(void* arg)
{
    ESP_LOGI(TAG, "Unoccupied timer expired. Setting state to UNOCCUPIED.");
    g_occupancy_state = false;
    if (g_pir_config.cb) {
        g_pir_config.cb(g_pir_config.endpoint_id, g_occupancy_state, g_pir_config.user_data);
    }
}

static void pir_sensor_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            bool current_level = gpio_get_level(io_num);
            ESP_LOGD(TAG, "GPIO[%lu] intr, val: %d. Current g_occupancy_state: %s", 
                     io_num, current_level, g_occupancy_state ? "OCCUPIED" : "UNOCCUPIED");

            if (current_level == 1) { // Motion detected (PIR output HIGH)
                if (!g_occupancy_state) {
                    ESP_LOGI(TAG, "Motion DETECTED. Setting state to OCCUPIED.");
                    g_occupancy_state = true;
                    if (g_pir_config.cb) {
                        g_pir_config.cb(g_pir_config.endpoint_id, g_occupancy_state, g_pir_config.user_data);
                    }
                }
                // Whether previously occupied or not, if motion is detected, (re)start the unoccupied timer.
                if (g_unoccupied_timer) {
                    uint16_t delay_seconds = get_pir_unoccupied_delay_seconds(g_pir_config.endpoint_id);
                    ESP_LOGI(TAG, "Using unoccupied delay of %u seconds from Matter attribute.", delay_seconds);
                    esp_timer_stop(g_unoccupied_timer);
                    esp_timer_start_once(g_unoccupied_timer, (uint64_t)delay_seconds * 1000000ULL);
                    ESP_LOGI(TAG, "Unoccupied timer (re)started for %u seconds.", delay_seconds);
                }
            } else { // Motion stopped (PIR output LOW - this happens after PIR's internal delay)
                // The PIR output going LOW doesn't immediately mean UNOCCUPIED from Matter's perspective.
                // The g_unoccupied_timer handles the transition to UNOCCUPIED.
                // We log this event for debugging but don't change g_occupancy_state here.
                uint16_t delay_seconds = get_pir_unoccupied_delay_seconds(g_pir_config.endpoint_id);
                ESP_LOGI(TAG, "PIR output LOW (raw signal). Occupancy state remains %s until %us timer expires.", 
                         g_occupancy_state ? "OCCUPIED" : "UNOCCUPIED", delay_seconds);
            }
        }
    }
}

esp_err_t pir_sensor_init(const pir_sensor_config_t *config)
{
    if (!config || !config->cb) {
        ESP_LOGE(TAG, "Invalid configuration or callback missing.");
        return ESP_ERR_INVALID_ARG;
    }
    g_pir_config = *config;

    ESP_LOGI(TAG, "Initializing PIR sensor on GPIO %d", PIR_SENSOR_GPIO_PIN);

    gpio_config_t io_conf = {};    
    io_conf.intr_type = GPIO_INTR_ANYEDGE; // Trigger on RISING (motion start) and FALLING (motion end for PIR)
    io_conf.pin_bit_mask = (1ULL << PIR_SENSOR_GPIO_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE; // Enable pull-down resistor
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configuring GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (!gpio_evt_queue) {
        ESP_LOGE(TAG, "Failed to create GPIO event queue");
        return ESP_FAIL;
    }

    // Start the PIR sensor task
    if (xTaskCreate(pir_sensor_task, "pir_sensor_task", 3072, NULL, 10, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create PIR sensor task");
        vQueueDelete(gpio_evt_queue);
        return ESP_FAIL;
    }

    // Initialize the one-shot timer for unoccupied delay
    const esp_timer_create_args_t unoccupied_timer_args = {
            .callback = &unoccupied_timer_callback,
            .name = "unoccupied-timer"
    };
    ret = esp_timer_create(&unoccupied_timer_args, &g_unoccupied_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create unoccupied timer: %s", esp_err_to_name(ret));
        // Proper cleanup of queue and task would be needed here
        return ret;
    }

    // Install gpio isr service
    ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1); // Using LEVEL1 for potentially faster response
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // ESP_ERR_INVALID_STATE means already installed
        ESP_LOGE(TAG, "Error installing GPIO ISR service: %s", esp_err_to_name(ret));
        vQueueDelete(gpio_evt_queue);
        // Potentially stop task if created, but it won't receive events.
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "GPIO ISR service already installed. Proceeding.");
    }

    // Hook isr handler for specific gpio pin
    ret = gpio_isr_handler_add(PIR_SENSOR_GPIO_PIN, gpio_isr_handler, (void*) PIR_SENSOR_GPIO_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error adding ISR handler for GPIO %d: %s", PIR_SENSOR_GPIO_PIN, esp_err_to_name(ret));
        // Clean up: remove ISR service if we exclusively installed it, delete queue, stop task
        if (ret != ESP_ERR_INVALID_STATE) gpio_uninstall_isr_service();
        vQueueDelete(gpio_evt_queue);
        // Need a way to stop the task here, or ensure it handles queue deletion gracefully.
        return ret;
    }

    ESP_LOGI(TAG, "PIR sensor initialized successfully. Unoccupied delay will be read from Matter attribute.");
    return ESP_OK;
}

// TODO:
// 1. Implement timer logic for "unoccupied" delay.
// 2. Integrate with Matter: call esp_matter_attribute_update when g_occupancy_state changes.
//    - Need to know the endpoint ID and cluster ID for OccupancySensing.
// 3. Add Kconfig option for PIR_SENSOR_GPIO_PIN (e.g., CONFIG_PIR_SENSOR_GPIO_NUM) 