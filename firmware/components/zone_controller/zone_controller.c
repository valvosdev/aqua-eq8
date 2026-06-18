#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "zone_controller.h"

static const char *TAG = "zone_controller";

typedef struct {
    bool is_manually_on;  
    bool is_master;       
} zone_state_t;

struct zone_controller_ctx_t {
    shift_reg_handle_t sr;
    adc_buttons_handle_t btn;
    zone_state_t zones[TOTAL_ZONES];
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    bool running;
    
    zone_event_cb_t cb_on;
    zone_event_cb_t cb_off;
    zone_event_cb_t cb_master;
    void *user_ctx;
};

static void update_hardware_outputs(struct zone_controller_ctx_t *ctx)
{
    if (!ctx || !ctx->sr) return;

    bool any_other_zone_active = false;
    int master_idx = -1;

    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // 1. Locate the master index and see if any NON-master zone is active
        for (int i = 0; i < TOTAL_ZONES; i++) {
            if (ctx->zones[i].is_master) {
                master_idx = i;
            } else if (ctx->zones[i].is_manually_on) {
                any_other_zone_active = true;
            }
        }

        // 2. Loop through all zones to compute precise Red and Green states
        for (uint8_t i = 0; i < TOTAL_ZONES; i++) {
            bool red_out = false;
            bool green_out = false;

            if (ctx->zones[i].is_master) {
                // MASTER LOGIC RULES:
                // Is it on? Yes, if directly pressed OR if any other channel forces it on
                bool master_is_on = ctx->zones[i].is_manually_on || any_other_zone_active;
                
                if (master_is_on) {
                    red_out = true;   // Engages Triac / Master valve
                    green_out = true; // Turn Green status light ON (Both Red & Green ON)
                }
            } else {
                // REGULAR ZONE LOGIC:
                if (ctx->zones[i].is_manually_on) {
                    red_out = true;   // Regular zone is manually active
                }
            }

            // Push state updates safely to your shift registers
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
    
    while (ctx->running) {
        update_hardware_outputs(ctx);
        vTaskDelay(pdMS_TO_TICKS(40)); // Smooth 25Hz refresh cycle
    }
    vTaskDelete(NULL);
}

void zone_controller_handle_button_event(zone_controller_handle_t handle, uint8_t button_index, adc_button_event_t event)
{
    struct zone_controller_ctx_t *ctx = (struct zone_controller_ctx_t *)handle;
    if (!ctx || button_index >= TOTAL_ZONES) return;

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (event == BUTTON_SHORT_PRESS) {
            ctx->zones[button_index].is_manually_on = !ctx->zones[button_index].is_manually_on;
            
            ESP_LOGI(TAG, "Zone %d manual state toggled to: %s", button_index, 
                     ctx->zones[button_index].is_manually_on ? "ON" : "OFF");

            if (ctx->zones[button_index].is_manually_on && ctx->cb_on) {
                ctx->cb_on(button_index, ctx->user_ctx);
            } else if (!ctx->zones[button_index].is_manually_on && ctx->cb_off) {
                ctx->cb_off(button_index, ctx->user_ctx);
            }

        } else if (event == BUTTON_LONG_PRESS) {
            for (int i = 0; i < TOTAL_ZONES; i++) {
                ctx->zones[i].is_master = (i == button_index);
            }
            
            ESP_LOGW(TAG, "Zone %d designated as System Master", button_index);
            if (ctx->cb_master) {
                ctx->cb_master(button_index, ctx->user_ctx);
            }
        }
        xSemaphoreGive(ctx->mutex);
    }
}

esp_err_t zone_controller_init(const zone_controller_config_t *config, zone_controller_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    struct zone_controller_ctx_t *ctx = calloc(1, sizeof(struct zone_controller_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "Context allocation failure");

    ctx->sr = config->sr_handle;
    ctx->btn = config->btn_handle;
    ctx->cb_on = config->on_zone_activated;
    ctx->cb_off = config->on_zone_deactivated;
    ctx->cb_master = config->on_master_assigned;
    ctx->user_ctx = config->user_ctx;
    ctx->running = true;

    ctx->mutex = xSemaphoreCreateMutex();
    if (!ctx->mutex) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_status = xTaskCreatePinnedToCore(
        zone_refresh_task, "zone_refresh_task", 3072, ctx, 4, &ctx->task_handle, 0
    );
    if (task_status != pdPASS) {
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
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "Null handle provided");
    
    ctx->running = false;
    vTaskDelay(pdMS_TO_TICKS(100));
    vSemaphoreDelete(ctx->mutex);
    free(ctx);
    return ESP_OK;
}
