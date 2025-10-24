#ifndef PIR_SENSOR_H
#define PIR_SENSOR_H

#include "esp_err.h"
#include <stdint.h> // For uint16_t
#include <stdbool.h> // For bool

// Define the GPIO pin for the PIR sensor input
#define PIR_SENSOR_GPIO_PIN CONFIG_PIR_SENSOR_GPIO_NUM // From Kconfig

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type for PIR sensor events.
 *
 * @param endpoint_id The Matter endpoint ID associated with this sensor.
 * @param occupancy True if occupied, false if not.
 * @param user_data User data pointer passed during initialization.
 */
typedef void (*pir_sensor_event_cb_t)(uint16_t endpoint_id, bool occupancy, void *user_data);

/**
 * @brief Configuration structure for the PIR sensor.
 */
typedef struct {
    pir_sensor_event_cb_t cb;       /**< Callback function to report occupancy changes. */
    uint16_t endpoint_id;         /**< Matter endpoint ID for this sensor. */
    void *user_data;              /**< Optional user data for the callback. */
    // We might add timer configuration here later (e.g., unoccupied_delay_sec)
} pir_sensor_config_t;

/**
 * @brief Initialize the PIR motion sensor
 *
 * Configures the GPIO pin for the PIR sensor and sets up interrupt handling.
 *
 * @param config Pointer to the PIR sensor configuration structure.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t pir_sensor_init(const pir_sensor_config_t *config);

/**
 * @brief Get the current occupancy state.
 * 
 * This function might be useful for direct polling if needed,
 * but primary updates will be event-driven to Matter.
 * 
 * @return true if motion is currently considered detected (based on PIR and internal logic),
 *         false otherwise.
 */
// bool pir_sensor_get_occupancy_state(void); // Placeholder for now

#ifdef __cplusplus
}
#endif

#endif // PIR_SENSOR_H 