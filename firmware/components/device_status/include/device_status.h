#ifndef DEVICE_STATUS_H
#define DEVICE_STATUS_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System Priority States for the Status LED
 */
typedef enum {
    STATUS_STATE_BOOTING = 0,
    STATUS_STATE_WIFI_CONNECTED,
    STATUS_STATE_BLE_ADVERTISING,
    STATUS_STATE_BLE_CONNECTED,
    STATUS_STATE_OTA_UPDATE,
    STATUS_STATE_ERROR,
    STATUS_STATE_MAX
} device_status_state_t;

/**
 * @brief Application Callback hooks for actions triggered by Button 9
 */
typedef struct {
    void (*on_short_press)(void);               /*!< Triggered on short press */
    void (*on_very_very_long_press)(void);     /*!< Triggered after 10-second hold */
} device_status_callbacks_t;

/**
 * @brief Hardware Configuration Struct for Status Manager
 */
typedef struct {
    gpio_num_t pin_btn;   /*!< Button Pin (GPIO_NUM_1) */
    gpio_num_t pin_red;   /*!< Red LED Pin (GPIO_NUM_2) */
    gpio_num_t pin_green; /*!< Green LED Pin (GPIO_NUM_3) */
    gpio_num_t pin_blue;  /*!< Blue LED Pin (GPIO_NUM_4) */
    device_status_callbacks_t callbacks;
} device_status_config_t;

typedef struct device_status_ctx_t* device_status_handle_t;

/**
 * @brief Initialize Button 9, PWM engines, and start background monitoring tasks
 */
esp_err_t device_status_init(const device_status_config_t *config, device_status_handle_t *out_handle);

/**
 * @brief Set or clear a specific priority state dynamically
 * 
 * @param handle Status manager instance handle
 * @param state The state to modify
 * @param active true to enable state, false to disable it
 */
esp_err_t device_status_set_state(device_status_handle_t handle, device_status_state_t state, bool active);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_STATUS_H
