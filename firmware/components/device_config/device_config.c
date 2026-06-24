#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "device_config.h"

static const char *TAG = "device_config_comp";
#define NVS_NAMESPACE "sprink_conf"

esp_err_t device_config_save_to_nvs(const device_runtime_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS storage namespace (%s)", esp_err_to_name(err));
        return err;
    }

    // Write String Blocks
    nvs_set_str(handle, "tenant_id", config->tenant_id);
    nvs_set_str(handle, "device_id", config->device_id);
    nvs_set_str(handle, "mqtt_url",  config->mqtt_url);
    nvs_set_str(handle, "ota_url", config->ota_url);
    nvs_set_str(handle, "timezone",  config->timezone);

    // Write Variable Numerical Metrics
    nvs_set_i8(handle, "master_ch", config->master_ch);
    nvs_set_u32(handle, "master_delay", config->master_delay_sec);

    // Hard Commit to internal silicon blocks
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configuration structural layout cleanly updated on flash partition.");
    }
    return err;
}

esp_err_t device_config_load_from_nvs(device_runtime_config_t *out_config)
{
    if (!out_config) return ESP_ERR_INVALID_ARG;
    memset(out_config, 0, sizeof(device_runtime_config_t));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Namespace unallocated or missing (%s). Restoring default structures.", esp_err_to_name(err));
        return err;
    }

    size_t size;
    
    size = sizeof(out_config->tenant_id);
    nvs_get_str(handle, "tenant_id", out_config->tenant_id, &size);
    
    size = sizeof(out_config->device_id);
    nvs_get_str(handle, "device_id", out_config->device_id, &size);
    
    size = sizeof(out_config->mqtt_url);
    nvs_get_str(handle, "mqtt_url", out_config->mqtt_url, &size);
    
    size = sizeof(out_config->timezone);
    nvs_get_str(handle, "timezone", out_config->timezone, &size);

    nvs_get_i8(handle, "master_ch", &out_config->master_ch);
    nvs_get_u32(handle, "master_delay", &out_config->master_delay_sec);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t device_config_update_from_json(const char *json_string, size_t length)
{
    if (!json_string || length == 0) return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_ParseWithLength(json_string, length);
    if (!root) {
        ESP_LOGE(TAG, "JSON string failed structural evaluation parser validation checks.");
        return ESP_ERR_INVALID_STATE;
    }

    // Capture existing snapshot state records first to preserve unchanged properties safely
    device_runtime_config_t working_config;
    device_config_load_from_nvs(&working_config);

    cJSON *item = NULL;

    // Safely pull string mappings
    if ((item = cJSON_GetObjectItem(root, "tenant_id")) && cJSON_IsString(item)) {
        strncpy(working_config.tenant_id, item->valuestring, sizeof(working_config.tenant_id) - 1);
    }
    if ((item = cJSON_GetObjectItem(root, "device_id")) && cJSON_IsString(item)) {
        strncpy(working_config.device_id, item->valuestring, sizeof(working_config.device_id) - 1);
    }
    if ((item = cJSON_GetObjectItem(root, "mqtt_url")) && cJSON_IsString(item)) {
        strncpy(working_config.mqtt_url, item->valuestring, sizeof(working_config.mqtt_url) - 1);
    }
    if ((item = cJSON_GetObjectItem(root, "timezone")) && cJSON_IsString(item)) {
        strncpy(working_config.timezone, item->valuestring, sizeof(working_config.timezone) - 1);
    }

    // Safely pull integer numbers
    if ((item = cJSON_GetObjectItem(root, "master_ch")) && cJSON_IsNumber(item)) {
        working_config.master_ch = (int8_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(root, "master_delay")) && cJSON_IsNumber(item)) {
        working_config.master_delay_sec = (uint32_t)item->valueint;
    }

    cJSON_Delete(root);

    // Flush and back up memory states straight to physical NVS segments
    return device_config_save_to_nvs(&working_config);
}