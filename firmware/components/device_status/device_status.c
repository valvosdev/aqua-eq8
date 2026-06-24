#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "device_status.h"

static const char *TAG = "device_status";

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define LEDC_MAX_DUTY ((1 << 10) - 1) // 1023

#define BTN_POLL_INTERVAL_MS 20
#define BLE_PAIRING_TIMEOUT_MS (10 * 60 * 1000) // 10 Minutes
#define RESET_LONG_PRESS_MS 10000               // 10 Seconds

typedef enum
{
    EFFECT_SOLID,
    EFFECT_BREATHING,
    EFFECT_FLASHING
} led_effect_t;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    led_effect_t effect;
} color_profile_t;

// State Profiles mapped explicitly by priority index order matching header enum
static const color_profile_t state_profiles[STATUS_STATE_MAX] = {
    [STATUS_STATE_BOOTING] = {255, 255, 255, EFFECT_BREATHING},     // White Breathing
    [STATUS_STATE_WIFI_CONNECTED] = {0, 255, 0, EFFECT_SOLID},      // Solid Green
    [STATUS_STATE_BLE_ADVERTISING] = {0, 0, 255, EFFECT_BREATHING}, // Blue Breathing
    [STATUS_STATE_BLE_CONNECTED] = {0, 0, 255, EFFECT_SOLID},       // Solid Blue
    [STATUS_STATE_OTA_UPDATE] = {255, 0, 255, EFFECT_BREATHING},    // Purple Breathing
    [STATUS_STATE_ERROR] = {255, 0, 0, EFFECT_FLASHING}             // Red Flashing
};

// Helper array to turn enum integers into readable text strings in logs
static const char *state_names[STATUS_STATE_MAX] = {
    [STATUS_STATE_BOOTING] = "BOOTING",
    [STATUS_STATE_WIFI_CONNECTED] = "WIFI_CONNECTED",
    [STATUS_STATE_BLE_ADVERTISING] = "BLE_ADVERTISING",
    [STATUS_STATE_BLE_CONNECTED] = "BLE_CONNECTED",
    [STATUS_STATE_OTA_UPDATE] = "OTA_UPDATE",
    [STATUS_STATE_ERROR] = "ERROR"};

struct device_status_ctx_t
{
    gpio_num_t pin_btn;
    ledc_channel_t ledc_ch[3]; // 0:R, 1:G, 2:B
    bool state_active[STATUS_STATE_MAX];
    device_status_callbacks_t cb;

    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    bool running;
    bool in_factory_reset_ui;
};

// Map values taking into account the Common Anode electrical setup (Inverted Duty)
static void set_led_rgb_raw(struct device_status_ctx_t *ctx, uint8_t r, uint8_t g, uint8_t b)
{
    // Math: Duty = MaxDuty - ((Color * MaxDuty) / 255)
    uint32_t duty_r = LEDC_MAX_DUTY - ((r * LEDC_MAX_DUTY) / 255);
    uint32_t duty_g = LEDC_MAX_DUTY - ((g * LEDC_MAX_DUTY) / 255);
    uint32_t duty_b = LEDC_MAX_DUTY - ((b * LEDC_MAX_DUTY) / 255);

    ledc_set_duty(LEDC_MODE, ctx->ledc_ch[0], duty_r);
    ledc_update_duty(LEDC_MODE, ctx->ledc_ch[0]);
    ledc_set_duty(LEDC_MODE, ctx->ledc_ch[1], duty_g);
    ledc_update_duty(LEDC_MODE, ctx->ledc_ch[1]);
    ledc_set_duty(LEDC_MODE, ctx->ledc_ch[2], duty_b);
    ledc_update_duty(LEDC_MODE, ctx->ledc_ch[2]);
}

static void status_engine_task(void *pvParameters)
{
    struct device_status_ctx_t *ctx = (struct device_status_ctx_t *)pvParameters;
    uint32_t btn_hold_time_ms = 0;
    uint32_t ble_pairing_timer_ms = 0;
    bool ble_pairing_active = false;
    uint32_t loop_tick = 0;

    while (ctx->running)
    {
        // --- 1. HANDLE BUTTON INPUT (Pull-up logic: 0 = Pressed, 1 = Open) ---
        if (gpio_get_level(ctx->pin_btn) == 0)
        {
            btn_hold_time_ms += BTN_POLL_INTERVAL_MS;

            if (btn_hold_time_ms >= RESET_LONG_PRESS_MS && !ctx->in_factory_reset_ui)
            {
                ctx->in_factory_reset_ui = true;
                ESP_LOGE(TAG, "!!! VERY VERY LONG PRESS TRIGGERED !!!");
                if (ctx->cb.on_very_very_long_press)
                    ctx->cb.on_very_very_long_press();
            }
        }
        else
        {
            if (btn_hold_time_ms >= 50 && btn_hold_time_ms < 1500)
            {
                ESP_LOGW(TAG, "Short press: Enabling BLE pairing window (10 mins)");
                device_status_set_state(ctx, STATUS_STATE_BLE_ADVERTISING, true);
                if (ctx->cb.on_short_press)
                    ctx->cb.on_short_press();
            }
            btn_hold_time_ms = 0;
        }
       

        // --- 2. PAIRING TIMEOUT COUNTER --- //TODO: move this from here, this is business logics related to button press. 
        // TODO: add a long press callback and move this to the long press call back 
        if (ble_pairing_active)
        {
            ble_pairing_timer_ms += BTN_POLL_INTERVAL_MS;
            if (ble_pairing_timer_ms >= BLE_PAIRING_TIMEOUT_MS)
            {
                ble_pairing_active = false;
                ESP_LOGI(TAG, "BLE Pairing window timed out.");
                device_status_set_state(ctx, STATUS_STATE_BLE_ADVERTISING, false);
            }
        }

        // --- 3. EVALUATE PRIORITY HIGHEST VISUAL STATE ---
        device_status_state_t active_state = STATUS_STATE_BOOTING;

        if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            for (int i = STATUS_STATE_MAX - 1; i >= 0; i--)
            {
                if (ctx->state_active[i])
                {
                    active_state = (device_status_state_t)i;
                    break;
                }
            }
            xSemaphoreGive(ctx->mutex);
        }

        color_profile_t current_profile = state_profiles[active_state];
        if (ctx->in_factory_reset_ui)
        {
            current_profile.r = 255;
            current_profile.g = 0;
            current_profile.b = 0;
            current_profile.effect = EFFECT_BREATHING;
        }

        // --- 4. EXECUTE LED LIGHT EFFECTS MATH ---
        float factor = 1.0f;
        switch (current_profile.effect)
        {
        case EFFECT_BREATHING:
            factor = (sinf((float)loop_tick * 0.05f) + 1.0f) / 2.0f;
            set_led_rgb_raw(ctx, (uint8_t)(current_profile.r * factor),
                            (uint8_t)(current_profile.g * factor),
                            (uint8_t)(current_profile.b * factor));
            break;

        case EFFECT_FLASHING:
            factor = (loop_tick % 10 < 5) ? 1.0f : 0.0f;
            set_led_rgb_raw(ctx, (uint8_t)(current_profile.r * factor),
                            (uint8_t)(current_profile.g * factor),
                            (uint8_t)(current_profile.b * factor));
            break;

        case EFFECT_SOLID:
        default:
            set_led_rgb_raw(ctx, current_profile.r, current_profile.g, current_profile.b);
            break;
        }

        loop_tick++;
        vTaskDelay(pdMS_TO_TICKS(BTN_POLL_INTERVAL_MS));
    }
    vTaskDelete(NULL);
}

esp_err_t device_status_init(const device_status_config_t *config, device_status_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    struct device_status_ctx_t *ctx = calloc(1, sizeof(struct device_status_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "Memory allocation failed");

    // CRITICAL HOTFIX: Create the mutex instantly before writing any values or setting default states
    ctx->mutex = xSemaphoreCreateMutex();
    if (!ctx->mutex)
    {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    ctx->pin_btn = config->pin_btn;
    ctx->cb = config->callbacks;
    ctx->running = true;
    ctx->state_active[STATUS_STATE_BOOTING] = true; // Default state on boot

    // Configure Button IO1 with internal pull-up resistor active
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << ctx->pin_btn),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&btn_conf));

    // Configure Shared LEDC Hardware PWM Timer Settings
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Array mappings connecting structural channels with physical target IO lines
    gpio_num_t pins[3] = {config->pin_red, config->pin_green, config->pin_blue};
    ctx->ledc_ch[0] = LEDC_CHANNEL_0;
    ctx->ledc_ch[1] = LEDC_CHANNEL_1;
    ctx->ledc_ch[2] = LEDC_CHANNEL_2;

    for (int i = 0; i < 3; i++)
    {
        ledc_channel_config_t ledc_channel = {
            .speed_mode = LEDC_MODE,
            .channel = ctx->ledc_ch[i],
            .timer_sel = LEDC_TIMER,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = pins[i],
            .duty = LEDC_MAX_DUTY, // Start completely dark (Common Anode 1023 == OFF)
            .hpoint = 0,
            .flags.output_invert = 1};
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    BaseType_t task_created = xTaskCreatePinnedToCore(
        status_engine_task, "status_engine_task", 3584, ctx, 8, &ctx->task_handle, 0);
    if (task_created != pdPASS)
    {
        vSemaphoreDelete(ctx->mutex);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t device_status_set_state(device_status_handle_t handle, device_status_state_t state, bool active)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");
    ESP_RETURN_ON_FALSE(state < STATUS_STATE_MAX, ESP_ERR_INVALID_ARG, TAG, "Invalid state range");

    struct device_status_ctx_t *ctx = (struct device_status_ctx_t *)handle;

    // Safety check for array string bounds mapping
    const char *state_str = state_names[state] ? state_names[state] : "UNKNOWN_STATE";

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE)
    {
        // Only log if the state is actually changing to prevent log flooding
        if (ctx->state_active[state] != active)
        {
            ctx->state_active[state] = active;
            ESP_LOGI(TAG, "Status changed -> State: [%s] is now %s", state_str, active ? "ACTIVE (ON)" : "INACTIVE (OFF)");
        }
        xSemaphoreGive(ctx->mutex);
    }
    return ESP_OK;
}