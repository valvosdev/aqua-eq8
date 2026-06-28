#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_sntp.h"
#include "esp_err.h"

#include "device_config.h"
#include "zone_controller.h"
#include "device_mqtt.h"
#include "device_schedule.h"

// Assembly mapping anchors for external TLS asset blobs
extern const uint8_t ca_crt_start[] asm("_binary_valvos_dev_ca_crt_start");
extern const uint8_t ca_crt_end[] asm("_binary_valvos_dev_ca_crt_end");
extern const uint8_t client_crt_start[] asm("_binary_client_crt_start");
extern const uint8_t client_crt_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_end");

static const char *TAG = "device_mqtt";

// Shared thread-safe component instances
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static zone_controller_handle_t s_attached_zones = NULL;

// Runtime calculated matching topics
static char *s_topic_zone_set = NULL;
static char *s_topic_master_set = NULL;

/**
 * @brief Evaluates an incoming MQTT payload and executes tasks on the zone hardware layer
 */
static void dispatch_incoming_message(const char *topic, int topic_len, const char *payload, int payload_len)
{
    if (!s_attached_zones)
    {
        ESP_LOGE(TAG, "Hardware core engine detached. Dropping routing entry execution.");
        return;
    }

    // Allocate a temporary stack buffer for safe string mutations
    char *topic_buf = malloc(topic_len + 1);
    if (!topic_buf)
        return;
    memcpy(topic_buf, topic, topic_len);
    topic_buf[topic_len] = '\0';

    device_runtime_config_t conf;
    if (device_config_load_from_nvs(&conf) != ESP_OK)
    {
        free(topic_buf);
        return;
    }

    // Precalculate standard routing base string elements
    char zone_prefix[256];
    char sched_prefix[256];
    snprintf(zone_prefix, sizeof(zone_prefix), "tenant/%s/device/%s/zone/", conf.tenant_id, conf.device_id);
    snprintf(sched_prefix, sizeof(sched_prefix), "tenant/%s/device/%s/schedule/", conf.tenant_id, conf.device_id);

    // --------------------------------------------------------------------
    // PATH 1: Zone Channel Interception (".../zone/<number>/set")
    // --------------------------------------------------------------------
    if (strncmp(topic_buf, zone_prefix, strlen(zone_prefix)) == 0)
    {
        int zone_num = atoi(topic_buf + strlen(zone_prefix));

        bool requested_state = false;
        if (strncmp(payload, "1", payload_len) == 0 || strncmp(payload, "ON", payload_len) == 0)
        {
            requested_state = true;
        }

        if (zone_num >= 1 && zone_num <= TOTAL_ZONES)
        {
            ESP_LOGI(TAG, "Zone Control -> Slot %d updated to target state: %d", zone_num, requested_state);
            zone_controller_set_zone(s_attached_zones, zone_num, requested_state);
        }
        else
        {
            ESP_LOGE(TAG, "Target hardware zone index is out-of-bounds: %d", zone_num);
        }
    }
    // --------------------------------------------------------------------
    // PATH 2: Master Controller Parameter Designation Overwrites (".../master/set")
    // --------------------------------------------------------------------
    else if (s_topic_master_set && strcmp(topic_buf, s_topic_master_set) == 0)
    {
        char val_buf[16] = {0};
        int slice_len = (payload_len < sizeof(val_buf) - 1) ? payload_len : sizeof(val_buf) - 1;
        memcpy(val_buf, payload, slice_len);

        int target_master = atoi(val_buf);
        if (target_master >= 0 && target_master <= TOTAL_ZONES)
        {
            ESP_LOGW(TAG, "System Overwrite -> Relocating master bus parameters to channel: %d", target_master);
            zone_controller_set_master(s_attached_zones, target_master);

            conf.master_ch = target_master;
            device_config_save_to_nvs(&conf);
        }
    }
    // --------------------------------------------------------------------
    // PATH 3: Scheduling Engine Array Sub-Routing (".../schedule/<index>/<action>")
    // --------------------------------------------------------------------
    else if (strncmp(topic_buf, sched_prefix, strlen(sched_prefix)) == 0)
    {
        int sched_index = atoi(topic_buf + strlen(sched_prefix));

        if (sched_index >= 0 && sched_index < 4)
        {
            const char *action_suffix = topic_buf + strlen(sched_prefix);
            while (*action_suffix != '/' && *action_suffix != '\0')
            {
                action_suffix++; // Advance index position marker offsets
            }

            if (strcmp(action_suffix, "/set") == 0)
            {
                ESP_LOGI(TAG, "Schedule Update -> Profile slot index: %d", sched_index);
                device_schedule_update_from_json(sched_index, payload, payload_len);
            }
            else if (strcmp(action_suffix, "/start") == 0)
            {
                ESP_LOGW(TAG, "Schedule Trigger -> Initialising execution timeline matrix on slot: %d", sched_index);
                device_schedule_start_by_index(sched_index);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Incoming instruction targeted an invalid schedule profile index: %d", sched_index);
        }
    }

    free(topic_buf);
}

/**
 * @brief MQTT Subscription Event Dispatcher Loop
 */
static void on_mqtt_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Cloud bridge established. Syncing routing tables...");

        if (s_topic_zone_set && s_topic_master_set)
        {
            esp_mqtt_client_subscribe(client, s_topic_zone_set, 1);
            esp_mqtt_client_subscribe(client, s_topic_master_set, 1);

            device_runtime_config_t conf;
            if (device_config_load_from_nvs(&conf) == ESP_OK)
            {
                char sched_wildcard[256];
                snprintf(sched_wildcard, sizeof(sched_wildcard), "tenant/%s/device/%s/schedule/#", conf.tenant_id, conf.device_id);
                esp_mqtt_client_subscribe(client, sched_wildcard, 1);

                ESP_LOGI(TAG, "Dynamic subscription bindings activated successfully.");
            }
        }
        break;

    case MQTT_EVENT_DATA:
        dispatch_incoming_message(event->topic, event->topic_len, event->data, event->data_len);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Broker link went offline. Reconnection worker active in background.");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Internal communication error context reported.");
        break;

    default:
        break;
    }
}

esp_err_t device_mqtt_start(const device_runtime_config_t *config, zone_controller_handle_t zone_engine)
{
    if (!config || !zone_engine) return ESP_ERR_INVALID_ARG;
    if (s_mqtt_client != NULL) return ESP_ERR_INVALID_STATE;

    s_attached_zones = zone_engine;

    // Allocate dynamic topics matching loaded profile values
    asprintf(&s_topic_zone_set, "tenant/%s/device/%s/zone/+/set", config->tenant_id, config->device_id);
    asprintf(&s_topic_master_set, "tenant/%s/device/%s/master/set", config->tenant_id, config->device_id);

    // FIX: Dynamically generate a clear, human-readable Client ID for Kubernetes logging
    char dynamic_client_id[128];
    snprintf(dynamic_client_id, sizeof(dynamic_client_id), "esp32-device-%s", config->device_id);

    // Calculate memory array bounds for custom asset binaries
    uint32_t ca_len = ca_crt_end - ca_crt_start;
    uint32_t client_crt_len = client_crt_end - client_crt_start;
    uint32_t client_key_len = client_key_end - client_key_start;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = config->mqtt_url,
            .verification = {
                .certificate = (const char *)ca_crt_start,
                .certificate_len = ca_len,
                .skip_cert_common_name_check = true,
            },
        },
        .credentials = {
            .client_id = dynamic_client_id, // <--- SET CLIENT ID HERE
            .authentication = {
                .certificate = (const char *)client_crt_start,
                .certificate_len = client_crt_len,
                .key = (const char *)client_key_start,
                .key_len = client_key_len,
            },
        },
        .session.keepalive = 15,
        .network.reconnect_timeout_ms = 10000,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        free(s_topic_zone_set); s_topic_zone_set = NULL;
        free(s_topic_master_set); s_topic_master_set = NULL;
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, on_mqtt_event, NULL);
    
    ESP_LOGI(TAG, "Booting MQTT engine using Client ID: %s", dynamic_client_id);
    return esp_mqtt_client_start(s_mqtt_client);
}

esp_err_t device_mqtt_stop(void)
{
    if (!s_mqtt_client)
        return ESP_ERR_INVALID_STATE;

    esp_mqtt_client_stop(s_mqtt_client);
    esp_mqtt_client_destroy(s_mqtt_client);
    s_mqtt_client = NULL;
    s_attached_zones = NULL;

    free(s_topic_zone_set);
    s_topic_zone_set = NULL;
    free(s_topic_master_set);
    s_topic_master_set = NULL;

    return ESP_OK;
}

esp_err_t device_mqtt_publish_status(const char *event_type, uint8_t channel_num)
{
    if (!s_mqtt_client)
        return ESP_ERR_INVALID_STATE;

    device_runtime_config_t conf;
    if (device_config_load_from_nvs(&conf) != ESP_OK)
        return ESP_FAIL;

    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "tenant/%s/device/%s/state", conf.tenant_id, conf.device_id);

    char payload[128];
    snprintf(payload, sizeof(payload), "{\"event\":\"%s\",\"channel\":%d}", event_type, channel_num);

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, state_topic, payload, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}