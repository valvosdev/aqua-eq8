#ifndef DEVICE_MQTT_H
#define DEVICE_MQTT_H

#include "esp_err.h"
#include "device_config.h"
#include "zone_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Allocates and fires up the background secure MQTT client connection worker
     * @note This function must only be executed AFTER network time synchronization completes.
     */
    esp_err_t device_mqtt_start(const device_runtime_config_t *config, zone_controller_handle_t zone_engine);

    /**
     * @brief Halts network pipelines and frees messaging tracking topics from system heap memory
     */
    esp_err_t device_mqtt_stop(void);

    /**
     * @brief Publishes telemetry runtime execution updates out to the tenant network cluster
     */
    esp_err_t device_mqtt_publish_status(const char *event_type, uint8_t channel_num);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_MQTT_H