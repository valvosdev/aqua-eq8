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

#ifdef __cplusplus
}
#endif

#endif // ZONE_CONTROLLER_H
