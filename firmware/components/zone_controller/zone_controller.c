#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "zone_controller.h"

static const char *TAG = "zone_controller";

#define REFRESH_PERIOD_MS 40                        // 25Hz execution speed
#define TICKS_PER_SECOND (1000 / REFRESH_PERIOD_MS) // 25 internal ticks = 1 second

typedef struct
{
    bool is_active;
    bool is_master;
    uint32_t delay_ticks_left;  // Tracks remaining master delay time in 25Hz steps
    bool is_blinking;          
} zone_state_t;

struct zone_controller_ctx_t
{
    shift_reg_handle_t sr;
    adc_buttons_handle_t btn;
    zone_state_t zones[TOTAL_ZONES];
    uint32_t global_master_delay_sec;
    int8_t active_master_idx;
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    bool running;
    uint32_t flash_timer_ticks;
    zone_event_cb_t cb_on;
    zone_event_cb_t cb_off;
    zone_event_cb_t cb_master;
    void *user_ctx;
};

static void update_hardware_outputs(struct zone_controller_ctx_t *ctx)
{
    if (!ctx || !ctx->sr) return;

    bool any_regular_zone_firing = false;
    bool master_delay_sequence_active = false;

    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        
        ctx->flash_timer_ticks++;

        // Pass 1: Handle time countdown adjustments across regular channels
        for (int i = 0; i < TOTAL_ZONES; i++) {
            if (ctx->zones[i].is_master) {
                continue;
            }

            if (ctx->zones[i].is_blinking && ctx->zones[i].delay_ticks_left > 0) {
                master_delay_sequence_active = true;
                ctx->zones[i].delay_ticks_left--;
                
                if (ctx->zones[i].delay_ticks_left == 0) {
                    ctx->zones[i].is_blinking = false;
                    ctx->zones[i].is_active = true; // Timer finished! Turn the physical zone ON
                    ESP_LOGI(TAG, "Master delay window closed. Regular Zone %d is now fully active.", i + 1);
                    if (ctx->cb_on) {
                        ctx->cb_on(i, ctx->user_ctx);
                    }
                }
            }

            if (ctx->zones[i].is_active) {
                any_regular_zone_firing = true;
            }
        }

        // Pass 2: Calculate and push Red & Green values to Shift Registers
        for (uint8_t i = 0; i < TOTAL_ZONES; i++) {
            bool red_out = false;
            bool green_out = false;

            if (ctx->zones[i].is_master) {
                // MASTER VALVE LOGIC RULES: 
                // Red & Green both turn solid ON if explicitly activated, or if any regular zone runs/waits
                if (ctx->zones[i].is_active || any_regular_zone_firing || master_delay_sequence_active) {
                    red_out = true;
                    green_out = true; 
                }
            } else {
                // REGULAR ZONE LOGIC FIXED:
                if (ctx->zones[i].is_active) {
                    red_out = true;    // Red ON when watering
                    green_out = false; // FIX: Green must stay OFF for regular active zones!
                } else if (ctx->zones[i].is_blinking) {
                    red_out = false;   // Relay remains open (closed valve) during delay windows
                    green_out = ((ctx->flash_timer_ticks / 3) % 2 == 0); // Green flashes at ~4Hz
                }
            }

            // Push calculation updates safely to your shift registers
            shift_reg_set_channel(ctx->sr, i,     red_out   ? SHIFT_REG_ON : SHIFT_REG_OFF);
            shift_reg_set_channel(ctx->sr, i + 8, green_out ? SHIFT_REG_ON : SHIFT_REG_OFF);
        }
        xSemaphoreGive(ctx->mutex);
    }
}

// Background task driving stable visual refreshes
static void zone_refresh_task(void *pvParameters)
{
    struct zone_controller_ctx_t *ctx = (struct zone_controller_ctx_t *)pvParameters;
    while (ctx->running)
    {
        update_hardware_outputs(ctx);
        vTaskDelay(pdMS_TO_TICKS(REFRESH_PERIOD_MS));
    }
    vTaskDelete(NULL);
}

/* ====================================================================
 * PUBLIC SETTERS & EXTERNAL API INTEGRATION (THREAD-SAFE)
 * ==================================================================== */

esp_err_t zone_controller_set_zone(zone_controller_handle_t handle, uint8_t zone_num, bool state)
{
    struct zone_controller_ctx_t *ctx = (struct zone_controller_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "Null handle configuration");
    ESP_RETURN_ON_FALSE(zone_num >= 1 && zone_num <= TOTAL_ZONES, ESP_ERR_INVALID_ARG, TAG, "Zone boundary error");

    uint8_t idx = zone_num - 1; // Map 1-8 down to 0-7 array indices

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE)
    {
        if (ctx->zones[idx].is_master)
        {
            ctx->zones[idx].is_active = state;
            ESP_LOGI(TAG, "Master Zone %d command tracking state changed to: %d", zone_num, state);
        }
        else
        {
            if (state)
            {
                // Check if a Master Delay sequence is required before activation
                if (ctx->global_master_delay_sec > 0 && ctx->active_master_idx >= 0)
                {
                    ctx->zones[idx].is_blinking = true;
                    ctx->zones[idx].delay_ticks_left = ctx->global_master_delay_sec * TICKS_PER_SECOND;
                    ctx->zones[idx].is_active = false;
                    ESP_LOGW(TAG, "Zone %d queued. Master valve warming up for %lu seconds...", zone_num, ctx->global_master_delay_sec);
                }
                else
                {
                    ctx->zones[idx].is_active = true;
                    ctx->zones[idx].is_blinking = false;
                    if (ctx->cb_on)
                        ctx->cb_on(idx, ctx->user_ctx);
                }
            }
            else
            {
                // Safe Shutdown Execution
                ctx->zones[idx].is_active = false;
                ctx->zones[idx].is_blinking = false;
                ctx->zones[idx].delay_ticks_left = 0;
                if (ctx->cb_off)
                    ctx->cb_off(idx, ctx->user_ctx);
            }
        }
        xSemaphoreGive(ctx->mutex);
    }
    return ESP_OK;
}

esp_err_t zone_controller_set_master(zone_controller_handle_t handle, uint8_t master_num)
{
    struct zone_controller_ctx_t *ctx = (struct zone_controller_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "Null handle configuration");
    ESP_RETURN_ON_FALSE(master_num <= TOTAL_ZONES, ESP_ERR_INVALID_ARG, TAG, "Master zone range validation error");

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        // Update our new fast index tracker variable safely
        if (master_num == 0) {
            ctx->active_master_idx = -1; // No master valve present
        } else {
            ctx->active_master_idx = (int8_t)(master_num - 1); // Map human 1-8 down to array 0-7
        }

        for (int i = 0; i < TOTAL_ZONES; i++) {
            ctx->zones[i].is_master = (master_num > 0 && i == (master_num - 1));
        }
        ESP_LOGW(TAG, "System configuration updated: Channel %d is now Exclusive Master.", master_num);
        if (master_num > 0 && ctx->cb_master) {
            ctx->cb_master(master_num, ctx->user_ctx); // Clean 1-indexed execution matching fix
        }
        xSemaphoreGive(ctx->mutex);
    }
    return ESP_OK;
}

esp_err_t zone_controller_set_master_delay(zone_controller_handle_t handle, uint32_t delay_seconds)
{
    struct zone_controller_ctx_t *ctx = (struct zone_controller_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "Null handle configuration");

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE)
    {
        ctx->global_master_delay_sec = delay_seconds;
        ESP_LOGI(TAG, "Master startup buffer window set to %lu seconds", delay_seconds);
        xSemaphoreGive(ctx->mutex);
    }
    return ESP_OK;
}

/* ====================================================================
 * ANALOG PHYSICAL BUTTON MATRIX INTERRUPT HANDLERS
 * ==================================================================== */

void zone_controller_handle_button_event(zone_controller_handle_t handle, uint8_t button_index, adc_button_event_t event)
{
    struct zone_controller_ctx_t *ctx = (struct zone_controller_ctx_t *)handle;
    if (!ctx || button_index >= TOTAL_ZONES)
        return;

    // Direct interface maps perfectly into our refined thread-safe setters
    if (event == BUTTON_SHORT_PRESS)
    {
        bool target_next_state = !ctx->zones[button_index].is_active && !ctx->zones[button_index].is_blinking;
        zone_controller_set_zone(handle, button_index + 1, target_next_state);
    }
    else if (event == BUTTON_LONG_PRESS)
    {
        zone_controller_set_master(handle, button_index + 1);
    }
}

/* ====================================================================
 * LIFECYCLE MANAGEMENT LAYER
 * ==================================================================== */

esp_err_t zone_controller_init(const zone_controller_config_t *config, zone_controller_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid initialization arguments");

    struct zone_controller_ctx_t *ctx = calloc(1, sizeof(struct zone_controller_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "Context memory allocation failure");

    ctx->sr = config->sr_handle;
    ctx->btn = config->btn_handle;
    ctx->cb_on = config->on_zone_activated;
    ctx->cb_off = config->on_zone_deactivated;
    ctx->cb_master = config->on_master_assigned;
    ctx->user_ctx = config->user_ctx;
    ctx->running = true;
    ctx->active_master_idx = -1;
    ctx->global_master_delay_sec = 5; // Default 5 seconds

    ctx->mutex = xSemaphoreCreateMutex();
    if (!ctx->mutex)
    {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_status = xTaskCreatePinnedToCore(
        zone_refresh_task, "zone_refresh_task", 3072, ctx, 4, &ctx->task_handle, 0);
    if (task_status != pdPASS)
    {
        vSemaphoreDelete(ctx->mutex);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }
    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t zone_controller_deinit(zone_controller_handle_t handle)
{
    struct zone_controller_ctx_t *ctx = (struct zone_controller_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "Null context tracking asset passed");
    ctx->running = false;
    vTaskDelay(pdMS_TO_TICKS(REFRESH_PERIOD_MS * 2));
    vSemaphoreDelete(ctx->mutex);
    free(ctx);
    return ESP_OK;
}