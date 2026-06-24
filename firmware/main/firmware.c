#include <time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"

// ESP-IDF v6 Unified Provisioning Headers
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_ble.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shift_reg_74hc595.h"
#include "adc_buttons.h"
#include "zone_controller.h"
#include "device_status.h"
#include "device_config.h"
#include "device_mqtt.h"

#define BTNS_PIN GPIO_NUM_0
#define LATCH GPIO_NUM_5
#define CLOCK GPIO_NUM_6
#define DATA GPIO_NUM_7

#define STATUS_BTN GPIO_NUM_1
#define STATUS_RED GPIO_NUM_2
#define STATUS_GREEN GPIO_NUM_3
#define STATUS_BLUE GPIO_NUM_4

#define PROV_POP "12345678"
#define PROV_PREFIX "VALVOS_AQUA8_"

static const char *APP_TAG = "app_main";
static zone_controller_handle_t global_zone_engine = NULL;
static device_status_handle_t global_status_engine = NULL;

// --- Pre-computed Cryptographic SRP6a Security 2 tokens for PIN "12345678" ---
static const char sec2_salt[] = {
    0xd2, 0xb4, 0xeb, 0xd0, 0x44, 0x6c, 0x03, 0x78, 0x24, 0x93, 0x21, 0x53, 0x33, 0xf3, 0x07, 0x61};

static const char sec2_verifier[] = {
    0x0c, 0xc4, 0xfd, 0x46, 0x5c, 0xb8, 0x0f, 0x0d, 0x37, 0x6e, 0x5e, 0xcb, 0x1f, 0x66, 0x2d, 0x05,
    0x7b, 0xfc, 0x2b, 0xfb, 0x1d, 0x9f, 0x77, 0x8f, 0x01, 0xe4, 0xe0, 0xea, 0xd3, 0x96, 0x00, 0x1d,
    0xc3, 0x14, 0x3c, 0x0b, 0x79, 0xe6, 0xe8, 0x2f, 0x5e, 0x73, 0xd9, 0xa2, 0x51, 0xa0, 0xa4, 0x10,
    0xac, 0x72, 0xcd, 0x59, 0x7a, 0x34, 0x57, 0x4b, 0xe9, 0x86, 0x28, 0x89, 0x83, 0x77, 0xb6, 0xde,
    0xd0, 0xed, 0x92, 0x9f, 0x73, 0xf1, 0xf9, 0x07, 0xa8, 0xed, 0xcf, 0x27, 0xff, 0x66, 0x2a, 0x4d,
    0x4e, 0x42, 0x0e, 0x03, 0xb4, 0x3f, 0xe3, 0x39, 0x81, 0x96, 0xfb, 0x3f, 0x35, 0x2d, 0x6a, 0x58,
    0x1c, 0x5a, 0x16, 0x74, 0xea, 0x01, 0x4f, 0xbe, 0xba, 0x41, 0x02, 0x38, 0x1c, 0x7a, 0x97, 0xf9,
    0x86, 0xbf, 0x0e, 0x1e, 0x4e, 0x50, 0xef, 0xb6, 0xac, 0x97, 0x62, 0x2c, 0xb2, 0x8f, 0x37, 0xea,
    0x56, 0x95, 0x4c, 0xc7, 0xa1, 0x2f, 0xb6, 0x6b, 0xd6, 0xab, 0x20, 0xbc, 0x1f, 0x65, 0x9e, 0x60,
    0x87, 0x27, 0x32, 0xf9, 0xbd, 0xfd, 0x76, 0x09, 0x24, 0x34, 0x5c, 0xbe, 0xb1, 0x47, 0x50, 0xf5,
    0xaf, 0xb6, 0x42, 0x5b, 0xab, 0xac, 0xf4, 0xe8, 0xf9, 0xaa, 0x58, 0xd1, 0x63, 0xda, 0x8a, 0xa3,
    0x8e, 0x5e, 0xec, 0x36, 0x55, 0x04, 0xe6, 0x57, 0x3d, 0xa0, 0x4b, 0xc4, 0x7c, 0xd8, 0xcc, 0xef,
    0x97, 0x53, 0xf7, 0x94, 0xcb, 0x69, 0x03, 0xaf, 0x74, 0xfa, 0x37, 0xa5, 0xd7, 0xd2, 0xa4, 0x6d,
    0x3d, 0x9e, 0x1b, 0xfa, 0x1b, 0xef, 0x87, 0x7e, 0x5f, 0xb6, 0x57, 0x70, 0x27, 0xfb, 0x65, 0x0a,
    0xef, 0xa2, 0x3f, 0xc3, 0xf8, 0xd7, 0x78, 0x71, 0x9d, 0x25, 0x08, 0x47, 0xd0, 0x5a, 0xa2, 0xbf,
    0x56, 0x95, 0x3f, 0x00, 0x52, 0x5d, 0x6d, 0x2a, 0xaf, 0x17, 0x0f, 0x87, 0x40, 0x33, 0x16, 0x7a,
    0x39, 0x20, 0x9f, 0xe7, 0x4d, 0x0b, 0x2b, 0x87, 0xc7, 0xc3, 0x96, 0xae, 0x29, 0xe8, 0xe2, 0x95,
    0x97, 0x08, 0xbe, 0x94, 0x90, 0x15, 0x61, 0xd9, 0xbd, 0x59, 0x72, 0x83, 0xd9, 0xa4, 0x1a, 0xf4,
    0x5f, 0x52, 0x0a, 0x4c, 0xac, 0xe6, 0xd0, 0x74, 0x2a, 0x56, 0x83, 0xe3, 0x49, 0xc0, 0x51, 0x29,
    0xb2, 0xac, 0xaf, 0xbd, 0x6f, 0x6b, 0x37, 0xfc, 0xc8, 0x1e, 0x49, 0xb4, 0xe3, 0x3d, 0x76, 0xd4,
    0xde, 0xcf, 0xc0, 0xbc, 0xc2, 0xaa, 0x2f, 0x98, 0x04, 0xa7, 0x63, 0xf7, 0xdb, 0x75, 0x03, 0xd4,
    0x4b, 0x55, 0x31, 0x5d, 0x97, 0x3b, 0x36, 0x94, 0x07, 0x3a, 0xb5, 0xb9, 0xcf, 0x76, 0x3f, 0x6a,
    0xe3, 0x0e, 0xb3, 0x40, 0xff, 0x40, 0x07, 0x94, 0xc3, 0x44, 0x9f, 0x28, 0x7b, 0x26, 0xbc, 0x16,
    0x4b, 0x76, 0x67, 0x25, 0xec, 0xbb, 0x38, 0x18, 0x17, 0x8e, 0x5f, 0xb6, 0x4e, 0x28, 0xe8, 0x30};

static void wifi_and_prov_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == NETWORK_PROV_EVENT)
    {
        switch (event_id)
        {
        case NETWORK_PROV_START:
            ESP_LOGI(APP_TAG, "Secure BLE Portal Active. Open ESP Prov App and use PIN 12345678.");
            device_status_set_state(global_status_engine, STATUS_STATE_BLE_ADVERTISING, true);
            break;
        case NETWORK_PROV_WIFI_CRED_RECV:
        {
            wifi_sta_config_t *wifi_config = (wifi_sta_config_t *)event_data;
            ESP_LOGI(APP_TAG, "Received encrypted configuration for SSID: %s", (char *)wifi_config->ssid);
            break;
        }
        case NETWORK_PROV_WIFI_CRED_FAIL:
            ESP_LOGE(APP_TAG, "Provisioning handshake authentication failed.");
            device_status_set_state(global_status_engine, STATUS_STATE_ERROR, true);
            break;
        case NETWORK_PROV_END:
            ESP_LOGI(APP_TAG, "Provisioning finalized cleanly. Releasing secure BLE layer.");
            device_status_set_state(global_status_engine, STATUS_STATE_BLE_ADVERTISING, false);
            network_prov_mgr_deinit();
            break;
        default:
            break;
        }
    }
    else if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(APP_TAG, "Wi-Fi link dropped. Reconnecting automatically...");
            device_status_set_state(global_status_engine, STATUS_STATE_WIFI_CONNECTED, false);
            esp_wifi_connect();
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(APP_TAG, "Connected securely! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));

        device_status_set_state(global_status_engine, STATUS_STATE_BOOTING, false);
        device_status_set_state(global_status_engine, STATUS_STATE_WIFI_CONNECTED, true);
        device_runtime_config_t active_net_conf;
        if (device_config_load_from_nvs(&active_net_conf) == ESP_OK)
        {

            // Pass the NVS variables directly to your MQTT startup component
            // Instead of a hardcoded placeholder, pass 'active_net_conf.mqtt_url'
            ESP_LOGI(APP_TAG, "Booting MQTT engine pointing to: %s", active_net_conf.mqtt_url);

            // Call your custom MQTT initialization method
            ESP_ERROR_CHECK(device_mqtt_init(global_zone_engine));
        }
        else
        {
            ESP_LOGE(APP_TAG, "Cannot start MQTT. No configuration profile found in NVS memory partitions.");
        }
    }
}

void sprinkler_on_handler(uint8_t zone, void *ctx)
{
    // The internal engine layer is 0-indexed (0-7).
    // Shift to 1-based index (1-8) for user readability and MQTT formatting alignment.
    uint8_t standard_zone = zone + 1;
    ESP_LOGW(APP_TAG, ">>> TRIAC ENGAGED: Sprinkler valve %d open! <<<", standard_zone);

    // Send visual state update packet out over the network automatically
    device_mqtt_publish_status("zone_on", standard_zone);
}

void sprinkler_off_handler(uint8_t zone, void *ctx)
{
    uint8_t standard_zone = zone + 1;
    ESP_LOGI(APP_TAG, ">>> TRIAC DISENGAGED: Sprinkler valve %d closed.", standard_zone);

    device_mqtt_publish_status("zone_off", standard_zone);
}

void master_assigned_handler(uint8_t zone, void *ctx)
{
    uint8_t human_zone = zone;
    ESP_LOGE(APP_TAG, "!!! Zone %d took Master control !!!", human_zone);

    device_runtime_config_t current_conf;

    // Attempt to load from NVS. If it fails because it's empty, initialize with defaults!
    esp_err_t err = device_config_load_from_nvs(&current_conf);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(APP_TAG, "NVS space empty. Initializing clean default configuration profile layout...");
        memset(&current_conf, 0, sizeof(device_runtime_config_t));
        // Assign basic default string bounds safely to prevent pointer corruption
        strcpy(current_conf.tenant_id, "default_tenant");
        strcpy(current_conf.device_id, "default_device");
        strcpy(current_conf.mqtt_url, "mqtt://localhost");
        current_conf.master_delay_sec = 5; // Default 5
    }

    // Always apply the new master channel change
    current_conf.master_ch = (int8_t)human_zone;
    current_conf.master_delay_sec = 5; // Default 5
    // Commit to flash partition
    err = device_config_save_to_nvs(&current_conf);
    if (err == ESP_OK)
    {
        ESP_LOGI(APP_TAG, "NVS Storage Updated: Master Valve saved as Channel %d.", human_zone);
    }
    else
    {
        ESP_LOGE(APP_TAG, "Failed to preserve master configuration into flash memory.");
    }

    // Broadcast the status update back to the network dashboard
    device_mqtt_publish_status("master_update", human_zone);
}

void app_button_bridge_handler(uint8_t button_index, adc_button_event_t event, void *user_ctx)
{
    zone_controller_handle_t engine = (zone_controller_handle_t)user_ctx;
    if (engine)
    {
        zone_controller_handle_button_event(engine, button_index, event);
    }
}

void pairing_mode_started(void)
{
    ESP_LOGW(APP_TAG, "Manual Pairing Window Requested via Button...");
    bool provisioned = false;
    network_prov_mgr_is_wifi_provisioned(&provisioned);

    if (!provisioned)
    {
        ESP_LOGI(APP_TAG, "Device is unconfigured. BLE portal is already active.");
    }
    else
    {
        ESP_LOGW(APP_TAG, "Re-launching Secure BLE Portal...");

        network_prov_security2_params_t sec2_params = {
            .salt = sec2_salt,
            .salt_len = sizeof(sec2_salt),
            .verifier = sec2_verifier,
            .verifier_len = sizeof(sec2_verifier),
        };

        network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_2, (const void *)&sec2_params, PROV_PREFIX, NULL);
    }
}

void factory_reset_wiping_sequence(void)
{
    ESP_LOGE(APP_TAG, "!!! FACTORY RESET ENGAGED: PURGING ALL ALLOCATED MEMORY PARTITIONS !!!");
    nvs_flash_erase();
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

esp_err_t custom_config_prov_handler(uint32_t session_id, const uint8_t *in_data, ssize_t in_len,
                                     uint8_t **out_data, ssize_t *out_len, void *priv_data)
{
    // One line completely handles validation, parsing, safety checks, and saving to flash memory
    esp_err_t err = device_config_update_from_json((const char *)in_data, in_len);
    if (err != ESP_OK)
        return ESP_FAIL;

    *out_data = (uint8_t *)strdup("{\"status\":\"config_applied\"}");
    *out_len = strlen((char *)*out_data);
    return ESP_OK;
}

void app_main(void)
{

    shift_reg_handle_t sr_device = NULL;
    adc_buttons_handle_t btn_device = NULL;
    device_runtime_config_t system_conf;
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    shift_reg_config_t sr_cfg = {
        .pin_ds = DATA,
        .pin_shcp = CLOCK,
        .pin_stcp = LATCH,
        .polarity = SHIFT_REG_INDIRECT};
    ESP_ERROR_CHECK(shift_reg_init(&sr_cfg, &sr_device));

    zone_controller_config_t zone_cfg = {
        .sr_handle = sr_device,
        .btn_handle = NULL,
        .on_zone_activated = sprinkler_on_handler,
        .on_zone_deactivated = sprinkler_off_handler,
        .on_master_assigned = master_assigned_handler,
        .user_ctx = NULL};
    ESP_ERROR_CHECK(zone_controller_init(&zone_cfg, &global_zone_engine));

    adc_buttons_config_t btn_cfg = {
        .gpio_num = BTNS_PIN,
        .short_press_threshold_ms = 50,
        .long_press_threshold_ms = 1200,
        .voltage_tolerance_mv = 75,
        .callback = app_button_bridge_handler,
        .user_ctx = global_zone_engine,
        .expected_mv = {4, 542, 938, 1249, 1502, 1701, 1866, 2007}};
    ESP_ERROR_CHECK(adc_buttons_init(&btn_cfg, &btn_device));

    device_status_config_t status_cfg = {
        .pin_btn = STATUS_BTN,
        .pin_red = STATUS_RED,
        .pin_green = STATUS_GREEN,
        .pin_blue = STATUS_BLUE,
        .callbacks = {
            .on_ble_pairing_start = pairing_mode_started,
            .on_factory_reset = factory_reset_wiping_sequence}};
    ESP_ERROR_CHECK(device_status_init(&status_cfg, &global_status_engine));

    // CALL THE COMPONENT METHOD TO READ FLASH HERE:
    if (device_config_load_from_nvs(&system_conf) == ESP_OK)
    {

        // Set your local system time-keeping zone profile
        if (strlen(system_conf.timezone) > 0)
        {
            setenv("TZ", system_conf.timezone, 1);
            tzset();
        }

        // Immediately apply NVS rules directly to the operational engine
        zone_controller_set_master(global_zone_engine, system_conf.master_ch);
        zone_controller_set_master_delay(global_zone_engine, system_conf.master_delay_sec);

        ESP_LOGI(APP_TAG, "NVS Config applied: Master CH=%d, Delay=%lu sec",
                 system_conf.master_ch, system_conf.master_delay_sec);
    }

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_and_prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_and_prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_and_prov_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    network_prov_mgr_config_t prov_config = {
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler = {
            .event_cb = network_prov_scheme_ble_event_cb_free_ble,
            .user_data = NULL}};
    ESP_ERROR_CHECK(network_prov_mgr_init(prov_config));

    
    bool provisioned = false;
    ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));

    if (!provisioned)
    {
        ESP_LOGW(APP_TAG, "Device not provisioned. Launching Secure BLE Provisioning Service...");

        uint8_t eth_mac[6] = {0};
        esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
        char custom_prov_name[32] = {0};
        snprintf(custom_prov_name, sizeof(custom_prov_name), "%s%02X%02X", PROV_PREFIX, eth_mac[4], eth_mac[5]);

        network_prov_security2_params_t sec2_params = {
            .salt = sec2_salt,
            .salt_len = sizeof(sec2_salt),
            .verifier = sec2_verifier,
            .verifier_len = sizeof(sec2_verifier),
        };

        ESP_ERROR_CHECK(network_prov_mgr_endpoint_create("custom-config"));
        // Start provisioning using standard Security 2 authentication token
        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_2, (const void *)&sec2_params, custom_prov_name, NULL));
        ESP_ERROR_CHECK(network_prov_mgr_endpoint_register("custom-config", custom_config_prov_handler, NULL));
    }
    else
    {
        network_prov_mgr_deinit();

        ESP_LOGI(APP_TAG, "Saved network credentials found. Autoconnecting to Wi-Fi...");
        device_status_set_state(global_status_engine, STATUS_STATE_BOOTING, false);
        esp_wifi_connect();
    }

    ESP_LOGI(APP_TAG, "Sprinkler Controller Online.");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
