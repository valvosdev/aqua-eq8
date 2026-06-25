#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Concrete application structural mirror mapping target properties
typedef struct {
    char tenant_id[64];
    char device_id[64];
    char mqtt_url[256];
    char ota_url[256];
    char timezone[64];
    int8_t master_ch;          // 0 = None, 1 to 8 = Dedicated Channel
    uint32_t master_delay_sec; // Startup pre-activation window delay countdown time
} device_runtime_config_t;

/**
 * @brief Commits a runtime configuration structure into local flash non-volatile storage partitions
 */
esp_err_t device_config_save_to_nvs(const device_runtime_config_t *config);

/**
 * @brief Loads preserved configuration records directly out of active flash memory layers
 */
esp_err_t device_config_load_from_nvs(device_runtime_config_t *out_config);

/**
 * @brief Parses an unparsed JSON text package, updates the active runtime target struct, and flushes to NVS
 */
esp_err_t device_config_update_from_json(const char *json_string, size_t length);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_CONFIG_H