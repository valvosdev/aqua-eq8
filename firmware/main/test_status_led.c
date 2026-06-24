#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEST_PIN_RED    GPIO_NUM_2   // Swap with your actual Red GPIO
#define TEST_PIN_GREEN  GPIO_NUM_3   // Swap with your actual Green GPIO

void app_main(void)
{
    ESP_LOGI("DIAGNOSTIC", "=== STARTING UNDER-CHIP BRIDGE TEST ===");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // -------------------------------------------------------------
    // TEST PHASE 1: Drive RED High, Read GREEN
    // -------------------------------------------------------------
    gpio_reset_pin(TEST_PIN_RED);
    gpio_reset_pin(TEST_PIN_GREEN);

    gpio_set_direction(TEST_PIN_RED,   GPIO_MODE_OUTPUT);
    gpio_set_direction(TEST_PIN_GREEN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TEST_PIN_GREEN, GPIO_FLOATING); // Turn off internal pullups

    gpio_set_level(TEST_PIN_RED, 1); // Drive Red to 3.3V
    vTaskDelay(pdMS_TO_TICKS(100));  // Allow lines to settle

    int reading_1 = gpio_get_level(TEST_PIN_GREEN);

    // -------------------------------------------------------------
    // TEST PHASE 2: Drive GREEN High, Read RED
    // -------------------------------------------------------------
    gpio_reset_pin(TEST_PIN_RED);
    gpio_reset_pin(TEST_PIN_GREEN);

    gpio_set_direction(TEST_PIN_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(TEST_PIN_RED,   GPIO_MODE_INPUT);
    gpio_set_pull_mode(TEST_PIN_RED,   GPIO_FLOATING);

    gpio_set_level(TEST_PIN_GREEN, 1); // Drive Green to 3.3V
    vTaskDelay(pdMS_TO_TICKS(100));

    int reading_2 = gpio_get_level(TEST_PIN_RED);

    // -------------------------------------------------------------
    // EVALUATION RESULTS
    // -------------------------------------------------------------
    ESP_LOGI("DIAGNOSTIC", "Results -> Phase 1 (Green Read): %d | Phase 2 (Red Read): %d", reading_1, reading_2);

    if (reading_1 == 1 && reading_2 == 1) {
        ESP_LOGE("DIAGNOSTIC", "❌ HARDWARE SHORT DETECTED!");
        ESP_LOGE("DIAGNOSTIC", "GPIO %d and GPIO %d are physically shorted together.", TEST_PIN_RED, TEST_PIN_GREEN);
        ESP_LOGE("DIAGNOSTIC", "This confirms a solder paste leak under the ESP32-C3-MINI module.");
    } else {
        ESP_LOGI("DIAGNOSTIC", "✅ PINS ARE ELECTRICALY ISOLATED.");
        ESP_LOGI("DIAGNOSTIC", "The ESP32 pins are clean. Look closely at the LED package itself.");
    }

    ESP_LOGI("DIAGNOSTIC", "=== DIAGNOSTIC COMPLETE ===");
}
