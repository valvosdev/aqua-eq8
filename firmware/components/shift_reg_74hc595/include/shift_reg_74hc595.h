#ifndef SHIFT_REG_74HC595_H
#define SHIFT_REG_74HC595_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Output orientation (polarity) logic
 */
typedef enum {
    SHIFT_REG_DIRECT = 0,   /*!< 1 logic level outputs High VCC (LED ON if cathode to GND) */
    SHIFT_REG_INDIRECT = 1  /*!< 1 logic level outputs Low GND (LED ON if anode to VCC) */
} shift_reg_polarity_t;

/**
 * @brief LED State modes
 */
typedef enum {
    SHIFT_REG_OFF = 0,
    SHIFT_REG_ON,
    SHIFT_REG_BLINK_SLOW,
    SHIFT_REG_BLINK_FAST
} shift_reg_mode_t;

/**
 * @brief Configuration structure for the shift register component
 */
typedef struct {
    gpio_num_t pin_ds;          /*!< Data Serial pin (DS / SER) */
    gpio_num_t pin_shcp;        /*!< Shift Register Clock pin (SH_CP / SRCLK) */
    gpio_num_t pin_stcp;        /*!< Storage Register Clock / Latch pin (ST_CP / RCLK) */
    shift_reg_polarity_t polarity; /*!< Direct or Indirect pin drive logic */
} shift_reg_config_t;

/**
 * @brief Opaque handle representing the shift register instance
 */
typedef struct shift_reg_ctx_t* shift_reg_handle_t;

/**
 * @brief Initialize the 74HC595 component driver
 * 
 * @param config Pointer to configuration struct
 * @param out_handle Pointer where the instance handle will be stored
 * @return esp_err_t ESP_OK on success
 */
esp_err_t shift_reg_init(const shift_reg_config_t *config, shift_reg_handle_t *out_handle);

/**
 * @brief Set the channel state for a specific channel (0-15)
 * Channels 0-7 map to the Red LEDs (Byte 1), channels 8-15 map to Green LEDs (Byte 2)
 * 
 * @param handle The shift register instance handle
 * @param channel Channel index (0 to 15)
 * @param mode Mode to execute (OFF, ON, BLINK_SLOW, BLINK_FAST)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t shift_reg_set_channel(shift_reg_handle_t handle, uint8_t channel, shift_reg_mode_t mode);

/**
 * @brief Deinitialize and clean up the shift register component
 * 
 * @param handle The shift register instance handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t shift_reg_deinit(shift_reg_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // SHIFT_REG_74HC595_H