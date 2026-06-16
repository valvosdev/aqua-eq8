#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "adc_buttons.h"

static const char *TAG = "adc_buttons";

#define DEBOUNCE_TICK_MS 20
#define NO_BUTTON_PRESSED 0xFF

typedef enum
{
    STATE_IDLE,
    STATE_PRESSED,
    STATE_LONG_PRESSED
} button_state_t;

struct adc_buttons_ctx_t
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_channel_t adc_chan;
    adc_cali_handle_t cali_handle;
    bool cali_enabled;

    uint32_t short_thresh;
    uint32_t long_thresh;
    uint16_t target_mv[ADC_BUTTONS_NUM];
    uint16_t tolerance;
    adc_button_callback_t cb;
    void *user_ctx;

    TaskHandle_t task_handle;
    bool running;
};

static uint8_t voltage_to_button_index(struct adc_buttons_ctx_t *ctx, int voltage_mv)
{
    if (voltage_mv > 3000)
    {
        return NO_BUTTON_PRESSED;
    }

    for (uint8_t i = 0; i < ADC_BUTTONS_NUM; i++)
    {
        int min_v = ctx->target_mv[i] - ctx->tolerance;
        int max_v = ctx->target_mv[i] + ctx->tolerance;
        if (voltage_mv >= min_v && voltage_mv <= max_v)
        {
            return i;
        }
    }
    return NO_BUTTON_PRESSED;
}

static void adc_buttons_task(void *pvParameters)
{
    struct adc_buttons_ctx_t *ctx = (struct adc_buttons_ctx_t *)pvParameters;
    uint8_t active_button = NO_BUTTON_PRESSED;
    button_state_t current_state = STATE_IDLE;
    uint32_t press_duration_ms = 0;

    while (ctx->running)
    {
        int raw_val = 0;
        int voltage_mv = 0;

        if (adc_oneshot_read(ctx->adc_handle, ctx->adc_chan, &raw_val) == ESP_OK)
        {
            if (ctx->cali_enabled)
            {
                adc_cali_raw_to_voltage(ctx->cali_handle, raw_val, &voltage_mv);
            }
            else
            {
                voltage_mv = (raw_val * 3300) / 4095;
            }
        }

        // >>> ADD THIS DIAGNOSTIC LOG LINE TEMPORARILY <<<
        // if (voltage_mv < 2800)
        // {
        //     ESP_LOGW("ADC_DEBUG", "Raw: %d, True Voltage: %d mV", raw_val, voltage_mv);
        // }

        uint8_t sampled_button = voltage_to_button_index(ctx, voltage_mv);

        switch (current_state)
        {
        case STATE_IDLE:
            if (sampled_button != NO_BUTTON_PRESSED)
            {
                active_button = sampled_button;
                press_duration_ms = 0;
                current_state = STATE_PRESSED;
            }
            break;

        case STATE_PRESSED:
            if (sampled_button == active_button)
            {
                press_duration_ms += DEBOUNCE_TICK_MS;
                if (press_duration_ms >= ctx->long_thresh)
                {
                    current_state = STATE_LONG_PRESSED;
                    if (ctx->cb)
                        ctx->cb(active_button, BUTTON_LONG_PRESS, ctx->user_ctx);
                }
            }
            else
            {
                if (press_duration_ms >= ctx->short_thresh)
                {
                    if (ctx->cb)
                        ctx->cb(active_button, BUTTON_SHORT_PRESS, ctx->user_ctx);
                }
                current_state = STATE_IDLE;
                active_button = NO_BUTTON_PRESSED;
            }
            break;

        case STATE_LONG_PRESSED:
            if (sampled_button != active_button)
            {
                current_state = STATE_IDLE;
                active_button = NO_BUTTON_PRESSED;
            }
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TICK_MS));
    }

    vTaskDelete(NULL);
}

esp_err_t adc_buttons_init(const adc_buttons_config_t *config, adc_buttons_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    esp_err_t ret = ESP_OK;

    adc_unit_t adc_unit;
    adc_channel_t adc_chan;
    esp_err_t err = adc_oneshot_io_to_channel(config->gpio_num, &adc_unit, &adc_chan);
    ESP_RETURN_ON_FALSE(err == ESP_OK, ESP_ERR_INVALID_ARG, TAG, "GPIO is not an ADC channel");

    struct adc_buttons_ctx_t *ctx = calloc(1, sizeof(struct adc_buttons_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "Memory allocation failed");

    ctx->short_thresh = config->short_press_threshold_ms;
    ctx->long_thresh = config->long_press_threshold_ms;
    ctx->tolerance = config->voltage_tolerance_mv;
    ctx->cb = config->callback;
    ctx->user_ctx = config->user_ctx;
    ctx->running = true;
    ctx->adc_chan = adc_chan;
    memcpy(ctx->target_mv, config->expected_mv, sizeof(ctx->target_mv));

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_GOTO_ON_ERROR(adc_oneshot_new_unit(&init_config, &ctx->adc_handle), err_clean_ctx, TAG, "Failed init ADC");

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_GOTO_ON_ERROR(adc_oneshot_config_channel(ctx->adc_handle, ctx->adc_chan, &chan_config), err_clean_unit, TAG, "Failed config channel");

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = adc_unit,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &ctx->cali_handle) == ESP_OK)
    {
        ctx->cali_enabled = true;
    }
#endif

    // FIX: Changed target execution core pin location from -1 to 0 for ESP32-C3 standard configurations
    BaseType_t task_status = xTaskCreatePinnedToCore(
        adc_buttons_task, "adc_buttons_task", 3072, ctx, 6, &ctx->task_handle, 0);
    if (task_status != pdPASS)
    {
        ret = ESP_ERR_NO_MEM;
        goto err_clean_cali;
    }

    *out_handle = ctx;
    return ESP_OK;

err_clean_cali:
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (ctx->cali_enabled)
        adc_cali_delete_scheme_line_fitting(ctx->cali_handle);
#endif
err_clean_unit:
    adc_oneshot_del_unit(ctx->adc_handle);
err_clean_ctx:
    free(ctx);
    return ret;
}

esp_err_t adc_buttons_deinit(adc_buttons_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Null handle provided");

    handle->running = false;
    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TICK_MS * 2));

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (handle->cali_enabled)
    {
        adc_cali_delete_scheme_line_fitting(handle->cali_handle);
    }
#endif

    adc_oneshot_del_unit(handle->adc_handle);
    free(handle);
    return ESP_OK;
}
