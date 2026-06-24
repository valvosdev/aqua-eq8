#ifndef ZONE_CONTROLLER_H
#define ZONE_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "shift_reg_74hc595.h"
#include "adc_buttons.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TOTAL_ZONES 8

typedef void (*zone_event_cb_t)(uint8_t zone_idx, void *user_ctx);

typedef struct {
    shift_reg_handle_t sr_handle;    
    adc_buttons_handle_t btn_handle; 
    
    zone_event_cb_t on_zone_activated;   
    zone_event_cb_t on_zone_deactivated; 
    zone_event_cb_t on_master_assigned;  
    void *user_ctx;                      
} zone_controller_config_t;

typedef struct zone_controller_ctx_t* zone_controller_handle_t;

esp_err_t zone_controller_init(const zone_controller_config_t *config, zone_controller_handle_t *out_handle);
esp_err_t zone_controller_deinit(zone_controller_handle_t handle);

/**
 * @brief Process button raw events and handle the system's zone states
 */
void zone_controller_handle_button_event(zone_controller_handle_t handle, uint8_t button_index, adc_button_event_t event);

/**
 * @brief Thread-safe API to explicitly drive a channel state (used by MQTT/Timers)
 * @note 1-based indices (1 to 8) to align cleanly with your MQTT spec.
 */
esp_err_t zone_controller_set_zone(zone_controller_handle_t handle, uint8_t zone_num, bool state);

/**
 * @brief Sets a specific channel as the master, clearing out old ones.
 * @note Accepts 0 for "None", or 1 to 8.
 */
esp_err_t zone_controller_set_master(zone_controller_handle_t handle, uint8_t master_num);

/**
 * @brief Configures the sequence pre-activation delay time.
 */
esp_err_t zone_controller_set_master_delay(zone_controller_handle_t handle, uint32_t delay_seconds);


#ifdef __cplusplus
}
#endif

#endif // ZONE_CONTROLLER_H
