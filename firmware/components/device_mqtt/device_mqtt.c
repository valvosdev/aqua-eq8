#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "device_config.h"
#include "zone_controller.h"
#include "device_mqtt.h"
#include "device_schedule.h"

static const char *TAG = "device_mqtt";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static zone_controller_handle_t attached_zone_engine = NULL;

// Dynamic memory paths constructed at runtime matching current loaded NVS profiles
static char *topic_zone_ctrl = NULL;
static char *topic_master_set = NULL;

/**
 * @brief Route raw incoming tenant payloads to the active hardware zone controller engine
 */
static void parse_tenant_mqtt_message(const char *topic, int topic_len, const char *data, int data_len) 
{
    if (!attached_zone_engine) return;

    // Fast bounds safety checks to prevent payload pointer runaways
    char *topic_buf = malloc(topic_len + 1);
    if (!topic_buf) return;
    memcpy(topic_buf, topic, topic_len);
    topic_buf[topic_len] = '\0';

    // Retrieve active runtime context structures to verify exact topic matches
    device_runtime_config_t conf;
    if (device_config_load_from_nvs(&conf) != ESP_OK) {
        free(topic_buf);
        return;
    }

    char zone_pattern[256]; 
    snprintf(zone_pattern, sizeof(zone_pattern), "tenant/%s/device/%s/zone/", conf.tenant_id, conf.device_id);

    // ====================================================================
    // 1. Check Zone Control Endpoint: "tenant/<tenant_id>/device/<device_id>/zone/+/set"
    // ====================================================================
    if (strncmp(topic_buf, zone_pattern, strlen(zone_pattern)) == 0) {
        int zone_num = atoi(topic_buf + strlen(zone_pattern));
        
        bool state = false;
        if (strncmp(data, "1", data_len) == 0 || strncmp(data, "ON", data_len) == 0) {
            state = true;
        }

        if (zone_num >= 1 && zone_num <= TOTAL_ZONES) {
            ESP_LOGI(TAG, "Tenant Verified Execution -> Zone %d command tracking state changed to: %d", zone_num, state);
            zone_controller_set_zone(attached_zone_engine, zone_num, state);
        } else {
            ESP_LOGE(TAG, "Tenant attempted to issue a command out of legal zone index bounds: %d", zone_num);
        }
    }
    // ====================================================================
    // 2. Check Master Overwrite Endpoint: "tenant/<tenant_id>/device/<device_id>/master/set"
    // ====================================================================
    else if (strcmp(topic_buf, topic_master_set) == 0) {
        char payload_buf[16] = {0};
        int length = (data_len < sizeof(payload_buf) - 1) ? data_len : sizeof(payload_buf) - 1;
        memcpy(payload_buf, data, length);
        
        int target_master = atoi(payload_buf);
        if (target_master >= 0 && target_master <= TOTAL_ZONES) {
            ESP_LOGW(TAG, "CRITICAL UPDATE: Overwriting master layout designation parameters to channel: %d", target_master);
            zone_controller_set_master(attached_zone_engine, target_master);
            
            // Save the dynamic master channel runtime override into NVS permanently
            conf.master_ch = target_master;
            device_config_save_to_nvs(&conf);
        }
    }
    // ====================================================================
    // ADDED HERE: 3. Check Schedule Configuration Endpoint: ".../schedule/<0-3>/set"
    // ====================================================================
    else {
        char sched_base_pattern[256];
        snprintf(sched_base_pattern, sizeof(sched_base_pattern), "tenant/%s/device/%s/schedule/", conf.tenant_id, conf.device_id);

        if (strncmp(topic_buf, sched_base_pattern, strlen(sched_base_pattern)) == 0) {
            // Extract the schedule profile index digit sitting directly after the string prefix block
            int sched_index = atoi(topic_buf + strlen(sched_base_pattern));

            if (sched_index >= 0 && sched_index < 4) { // MAX_SCHEDULES is 4 (0 to 3)
                // Locate the exact matching path tail string suffix using basic pointer offset calculations
                const char *action_suffix = topic_buf + strlen(sched_base_pattern);
                while (*action_suffix != '/' && *action_suffix != '\0') {
                    action_suffix++; // Step past the numeric index integer character
                }

                // A. Match Route Pattern: "tenant/<tenant_id>/device/<device_id>/schedule/<index>/set"
                if (strcmp(action_suffix, "/set") == 0) {
                    ESP_LOGI(TAG, "MQTT Command: Modifying Schedule Configuration profile slot %d...", sched_index + 1);
                    device_schedule_update_from_json(sched_index, data, data_len);
                }
                // B. Match Route Pattern: "tenant/<tenant_id>/device/<device_id>/schedule/<index>/start"
                else if (strcmp(action_suffix, "/start") == 0) {
                    ESP_LOGW(TAG, "MQTT Command: Remotely starting Schedule sequence profile slot %d!", sched_index + 1);
                    device_schedule_start_by_index(sched_index);
                }
            } else {
                ESP_LOGE(TAG, "Tenant requested an invalid out-of-bounds schedule array index: %d", sched_index);
            }
        }
    }

    free(topic_buf);
}

/**
 * @brief Managed component background subscription event handler loop
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t current_client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Secure cloud connection confirmed. Dispatching topic dynamic bindings...");

            if (topic_zone_ctrl && topic_master_set) {
                // Subscribe to zone and master commands
                esp_mqtt_client_subscribe(current_client, topic_zone_ctrl, 1);
                esp_mqtt_client_subscribe(current_client, topic_master_set, 1);
                
                // --- FIX: Load configuration from NVS inside this function's scope ---
                device_runtime_config_t conf;
                if (device_config_load_from_nvs(&conf) == ESP_OK) {
                    
                    char topic_sched_wildcard[256]; // Ensure buffer is large enough
                    snprintf(topic_sched_wildcard, sizeof(topic_sched_wildcard), 
                             "tenant/%s/device/%s/schedule/#", conf.tenant_id, conf.device_id);
                    
                    esp_mqtt_client_subscribe(current_client, topic_sched_wildcard, 1);
                    
                    ESP_LOGI(TAG, "Listening on paths:\n -> %s\n -> %s\n -> %s", 
                             topic_zone_ctrl, topic_master_set, topic_sched_wildcard);
                } else {
                    ESP_LOGE(TAG, "Failed to load NVS config for wildcard subscription.");
                }
            }
            break;

        case MQTT_EVENT_DATA:
            parse_tenant_mqtt_message(event->topic, event->topic_len, event->data, event->data_len);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Broker connection link lost. Network module attempting automated background recovery...");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Internal client messaging fault loop exception context reported.");
            break;

        default:
            break;
    }
}

esp_err_t device_mqtt_init(zone_controller_handle_t zone_engine)
{
    if (!zone_engine) return ESP_ERR_INVALID_ARG;
    attached_zone_engine = zone_engine;

    // Load active profile metrics from NVS memory partitions dynamically
    device_runtime_config_t config;
    esp_err_t err = device_config_load_from_nvs(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Aborting connection engine boot sequence: Configuration layout unallocated in NVS flash memory.");
        return err;
    }

    // Allocate and generate isolated tenant topics matching NVS properties
    asprintf(&topic_zone_ctrl, "tenant/%s/device/%s/zone/+/set", config.tenant_id, config.device_id);
    asprintf(&topic_master_set, "tenant/%s/device/%s/master/set", config.tenant_id, config.device_id);

    // Structure configuration blocks using modern structural group guidelines (mandatory since late v5.x)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config.mqtt_url,
        .network.reconnect_timeout_ms = 30000,
        // Optional Multi-Tenant Authentication Layer Mapping:
        //.credentials.username = config.tenant_id,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        free(topic_zone_ctrl); free(topic_master_set);
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(mqtt_client);
}

esp_err_t device_mqtt_deinit(void)
{
    if (!mqtt_client) return ESP_ERR_INVALID_STATE;

    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;

    free(topic_zone_ctrl); topic_zone_ctrl = NULL;
    free(topic_master_set); topic_master_set = NULL;

    return ESP_OK;
}

esp_err_t device_mqtt_publish_status(const char *event_type, uint8_t channel_num)
{
    if (!mqtt_client) return ESP_ERR_INVALID_STATE;

    // Load active configuration profiles from NVS to dynamically build the topic path
    device_runtime_config_t conf;
    if (device_config_load_from_nvs(&conf) != ESP_OK) return ESP_FAIL;

    // Build the reporting topic: "tenant/<tenant_id>/device/<device_id>/state"
    char state_topic[192];
    snprintf(state_topic, sizeof(state_topic), "tenant/%s/device/%s/state", conf.tenant_id, conf.device_id);

    // Build a compact, clean JSON payload string
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"event\":\"%s\",\"channel\":%d}", event_type, channel_num);

    // Publish to the broker (QoS 1 guarantees the dashboard receives it)
    int msg_id = esp_mqtt_client_publish(mqtt_client, state_topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish telemetry update status.");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Telemetry dispatched to -> %s: %s", state_topic, payload);
    return ESP_OK;
}