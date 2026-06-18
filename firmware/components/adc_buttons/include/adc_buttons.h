#ifndef ADC_BUTTONS_H
#define ADC_BUTTONS_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_BUTTONS_NUM 8

/**
 * @brief Button click event types
 */
typedef enum {
    BUTTON_SHORT_PRESS,   /*!< Button pressed and released quickly */
    BUTTON_LONG_PRESS     /*!< Button held past the long press threshold */
} adc_button_event_t;

/**
 * @brief Callback function definition for button actions
 */
typedef void (*adc_button_callback_t)(uint8_t button_index, adc_button_event_t event, void *user_ctx);

/**
 * @brief Configuration structure for the ADC Button array
 */
typedef struct {
    gpio_num_t gpio_num;               /*!< The GPIO pin connected to the button ladder (e.g. GPIO_NUM_0) */
    uint32_t short_press_threshold_ms; /*!< Min duration for valid press (e.g. 50ms) */
    uint32_t long_press_threshold_ms;  /*!< Duration to qualify a long press (e.g. 1000ms) */
    uint16_t expected_mv[ADC_BUTTONS_NUM]; /*!< Expected voltage in millivolts for each button */
    uint16_t voltage_tolerance_mv;     /*!< Allowed millivolt window variance (+/-) */
    adc_button_callback_t callback;    /*!< Event notification handler */
    void *user_ctx;                    /*!< User context forwarded to callback */
} adc_buttons_config_t;

/**
 * @brief Opaque handle representing the button controller instance
 */
typedef struct adc_buttons_ctx_t* adc_buttons_handle_t;

/**
 * @brief Initialize the ADC Button driver component
 * 
 * @param config Pointer to user setup parameters
 * @param out_handle Destination address for the instance handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t adc_buttons_init(const adc_buttons_config_t *config, adc_buttons_handle_t *out_handle);

/**
 * @brief Stop sampling and tear down the button component
 * 
 * @param handle Button array instance handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t adc_buttons_deinit(adc_buttons_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // ADC_BUTTONS_H