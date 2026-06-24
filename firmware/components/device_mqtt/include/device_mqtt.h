#ifndef DEVICE_MQTT_H
#define DEVICE_MQTT_H

#include "esp_err.h"
#include "zone_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and starts the background MQTT service engine.
 * 
 * @param zone_engine Pre-initialized handle to your physical hardware zone engine.
 * @return esp_err_t ESP_OK on initialization success, failure code otherwise.
 */
esp_err_t device_mqtt_init(zone_controller_handle_t zone_engine);

esp_err_t device_mqtt_publish_status(const char *event_type, uint8_t channel_num);

/**
 * @brief Stops and cleanly shuts down active background MQTT connections.
 */
esp_err_t device_mqtt_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_MQTT_H