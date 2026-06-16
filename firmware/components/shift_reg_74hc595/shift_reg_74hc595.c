#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_rom_sys.h" 
#include "shift_reg_74hc595.h"

static const char *TAG = "74hc595";

#define BLINK_SLOW_PERIOD_MS 1000
#define BLINK_FAST_PERIOD_MS 250
#define TICK_INTERVAL_MS      50   

struct shift_reg_ctx_t {
    gpio_num_t pin_ds;
    gpio_num_t pin_shcp;
    gpio_num_t pin_stcp;
    shift_reg_polarity_t polarity;
    shift_reg_mode_t channel_modes[16]; // FIX: Changed from single enum to explicit 16-channel array
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    bool running;
};

static void shift_reg_write_hardware(struct shift_reg_ctx_t *ctx, uint16_t bitmask)
{
    if (ctx->polarity == SHIFT_REG_INDIRECT) {
        bitmask = ~bitmask;
    }

    for (int i = 15; i >= 0; i--) {
        bool bit = (bitmask >> i) & 0x01;
        gpio_set_level(ctx->pin_ds, bit);

        gpio_set_level(ctx->pin_shcp, 1);
        esp_rom_delay_us(1); 
        gpio_set_level(ctx->pin_shcp, 0);
        esp_rom_delay_us(1);
    }

    gpio_set_level(ctx->pin_stcp, 1);
    esp_rom_delay_us(1);
    gpio_set_level(ctx->pin_stcp, 0);
}

static void shift_reg_task(void *pvParameters)
{
    struct shift_reg_ctx_t *ctx = (struct shift_reg_ctx_t *)pvParameters;
    uint32_t elapsed_ms = 0;
    bool blink_slow_state = false;
    bool blink_fast_state = false;

    while (ctx->running) {
        uint16_t current_output_mask = 0;

        if (elapsed_ms % (BLINK_SLOW_PERIOD_MS / 2) == 0) {
            blink_slow_state = !blink_slow_state;
        }
        if (elapsed_ms % (BLINK_FAST_PERIOD_MS / 2) == 0) {
            blink_fast_state = !blink_fast_state;
        }

        if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (int i = 0; i < 16; i++) {
                bool pin_on = false;
                switch (ctx->channel_modes[i]) {
                    case SHIFT_REG_ON:
                        pin_on = true;
                        break;
                    case SHIFT_REG_BLINK_SLOW:
                        pin_on = blink_slow_state;
                        break;
                    case SHIFT_REG_BLINK_FAST:
                        pin_on = blink_fast_state;
                        break;
                    case SHIFT_REG_OFF:
                    default:
                        pin_on = false;
                        break;
                }
                if (pin_on) {
                    current_output_mask |= (1 << i);
                }
            }
            xSemaphoreGive(ctx->mutex);
        }

        shift_reg_write_hardware(ctx, current_output_mask);

        vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL_MS));
        elapsed_ms += TICK_INTERVAL_MS;
    }

    vTaskDelete(NULL);
}

esp_err_t shift_reg_init(const shift_reg_config_t *config, shift_reg_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    struct shift_reg_ctx_t *ctx = calloc(1, sizeof(struct shift_reg_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");

    ctx->pin_ds = config->pin_ds;
    ctx->pin_shcp = config->pin_shcp;
    ctx->pin_stcp = config->pin_stcp;
    ctx->polarity = config->polarity;
    ctx->running = true;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ctx->pin_ds) | (1ULL << ctx->pin_shcp) | (1ULL << ctx->pin_stcp),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        free(ctx);
        return err;
    }

    gpio_set_level(ctx->pin_ds, 0);
    gpio_set_level(ctx->pin_shcp, 0);
    gpio_set_level(ctx->pin_stcp, 0);

    ctx->mutex = xSemaphoreCreateMutex();
    if (!ctx->mutex) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    memset(ctx->channel_modes, 0, sizeof(ctx->channel_modes));
    shift_reg_write_hardware(ctx, 0x0000);

    // FIX: Changed core mapping target from -1 to 0 for single-core ESP32-C3 units
    BaseType_t task_created = xTaskCreatePinnedToCore(
        shift_reg_task, "shift_reg_task", 3072, ctx, 5, &ctx->task_handle, 0
    );

    if (task_created != pdPASS) {
        vSemaphoreDelete(ctx->mutex);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t shift_reg_set_channel(shift_reg_handle_t handle, uint8_t channel, shift_reg_mode_t mode)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Handle cannot be null");
    ESP_RETURN_ON_FALSE(channel < 16, ESP_ERR_INVALID_ARG, TAG, "Channel out of range (0-15)");

    struct shift_reg_ctx_t *ctx = (struct shift_reg_ctx_t *)handle;
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        ctx->channel_modes[channel] = mode;
        xSemaphoreGive(ctx->mutex);
    }

    return ESP_OK;
}

esp_err_t shift_reg_deinit(shift_reg_handle_t handle)
{
    struct shift_reg_ctx_t *ctx = (struct shift_reg_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "Handle cannot be null");

    ctx->running = false;
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL_MS * 2));

    shift_reg_write_hardware(ctx, 0x0000);
    vSemaphoreDelete(ctx->mutex);
    free(ctx);
    
    return ESP_OK;
}
