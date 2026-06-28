#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "device_schedule.h"

static const char *TAG = "device_schedule";
#define NVS_SCHED_NAMESPACE "sprink_sched"

static zone_controller_handle_t attached_engine = NULL;
static schedule_storage_t system_schedules;
static SemaphoreHandle_t sched_mutex = NULL;
static TaskHandle_t execution_task_handle = NULL;
static bool sequence_running = false;


// Loads values from NVS or applies factory defaults if empty
static void load_schedules_from_nvs_or_default(void)
{
    nvs_handle_t handle;
    size_t required_size = sizeof(schedule_storage_t);

    if (nvs_open(NVS_SCHED_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK)
    {
        esp_err_t err = nvs_get_blob(handle, "config_blob", &system_schedules, &required_size);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "NVS empty. Generating baseline default 7-min sequence layout...");
            memset(&system_schedules, 0, sizeof(schedule_storage_t));

            // Generate standard Profile 1 Factory Defaults
            schedule_profile_t *p1 = &system_schedules.profiles[0];
            strncpy(p1->name, "Factory Default", SCHEDULE_NAME_LEN - 1);
            p1->is_active = true;
            p1->has_time_trigger = false;  // Initial profile cannot be triggered by time until set
            p1->interval_between_sec = 20; // 20-second gap between moving channels
            p1->total_steps = 8;

            for (int i = 0; i < 8; i++)
            {
                p1->steps[i].zone_num = i + 1;
                p1->steps[i].runtime_seconds = 420; // 7 minutes
            }

            nvs_set_blob(handle, "config_blob", &system_schedules, sizeof(schedule_storage_t));
            nvs_commit(handle);
        }
        nvs_close(handle);
    }
}

// Background worker managing sequential countdown timers
static void sequence_runner_task(void *pvParameters)
{
    uint8_t index = (uint8_t)(uintptr_t)pvParameters;
    schedule_profile_t active_profile;

    xSemaphoreTake(sched_mutex, portMAX_DELAY);
    memcpy(&active_profile, &system_schedules.profiles[index], sizeof(schedule_profile_t));
    sequence_running = true;
    xSemaphoreGive(sched_mutex);

    ESP_LOGW(TAG, "EXECUTING SCHEDULE SEQUENCE: [%s]", active_profile.name);

    for (int i = 0; i < active_profile.total_steps; i++)
    {
        uint8_t current_zone = active_profile.steps[i].zone_num;
        uint32_t active_run = active_profile.steps[i].runtime_seconds;

        ESP_LOGI(TAG, "Step %d/%d -> Activating Zone %d for %lu seconds", i + 1, active_profile.total_steps, current_zone, active_run);

        // Open the target valve relay cleanly via our public driver API
        zone_controller_set_zone(attached_engine, current_zone, true);

        // Count down the target watering runtime
        vTaskDelay(pdMS_TO_TICKS(active_run * 1000));

        // Close the target valve relay cleanly
        zone_controller_set_zone(attached_engine, current_zone, false);

        // If this isn't the last channel, run the soak/rest interval gap delay
        if (i < (active_profile.total_steps - 1) && active_profile.interval_between_sec > 0)
        {
            ESP_LOGW(TAG, "Soak Interval active. Waiting out %lu second inter-channel buffer window...", active_profile.interval_between_sec);
            vTaskDelay(pdMS_TO_TICKS(active_profile.interval_between_sec * 1000));
        }
    }

    ESP_LOGI(TAG, "Schedule sequence completed successfully.");
    sequence_running = false;
    execution_task_handle = NULL;
    vTaskDelete(NULL);
}

// Background daemon validating clock times every minute
static void time_trigger_watchdog_task(void *pvParameters)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(60000)); // Check time every 60 seconds

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // REQUIREMENT: Validate system time is synchronized with network before performing action
        // In POSIX/ESP-IDF, tm_year starts from 1900. If year is < 2020, time is un-synced/lost.
        if (timeinfo.tm_year < 120)
        {
            continue;
        }

        if (xSemaphoreTake(sched_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (!sequence_running)
            {
                for (uint8_t i = 0; i < MAX_SCHEDULES; i++)
                {
                    schedule_profile_t *p = &system_schedules.profiles[i];
                    if (p->is_active && p->has_time_trigger)
                    {
                        if (timeinfo.tm_hour == p->start_hour && timeinfo.tm_min == p->start_minute)
                        {
                            ESP_LOGW(TAG, "Time trigger matched for profile %d! Dispatched asynchronously.", i);
                            xTaskCreate(sequence_runner_task, "seq_run", 4096, (void *)(uintptr_t)i, 5, &execution_task_handle);
                            break;
                        }
                    }
                }
            }
            xSemaphoreGive(sched_mutex);
        }
    }
}

esp_err_t device_schedule_init(zone_controller_handle_t zone_engine)
{
    if (!zone_engine)
        return ESP_ERR_INVALID_ARG;
    attached_engine = zone_engine;

    sched_mutex = xSemaphoreCreateMutex();
    if (!sched_mutex)
        return ESP_ERR_NO_MEM;

    load_schedules_from_nvs_or_default();

    xTaskCreate(time_trigger_watchdog_task, "sched_watch", 3072, NULL, 4, NULL);
    return ESP_OK;
}

esp_err_t device_schedule_start_by_index(uint8_t index)
{
    if (index >= MAX_SCHEDULES)
        return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(sched_mutex, portMAX_DELAY);
    if (sequence_running)
    {
        ESP_LOGE(TAG, "Cannot spin up schedule sequence: Another profile is active.");
        xSemaphoreGive(sched_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    xTaskCreate(sequence_runner_task, "seq_run", 4096, (void *)(uintptr_t)index, 5, &execution_task_handle);
    xSemaphoreGive(sched_mutex);
    return ESP_OK;
}

void device_schedule_stop_current(void)
{
    xSemaphoreTake(sched_mutex, portMAX_DELAY);
    if (sequence_running && execution_task_handle != NULL)
    {
        vTaskDelete(execution_task_handle);
        execution_task_handle = NULL;
        sequence_running = false;

        // Emergency clean loop shutdown: Shut off all physical water zones instantly
        for (int i = 1; i <= 8; i++)
        {
            zone_controller_set_zone(attached_engine, i, false);
        }
        ESP_LOGE(TAG, "Emergency Sequence Override Triggered: All active valves closed.");
    }
    xSemaphoreGive(sched_mutex);
}

esp_err_t device_schedule_update_from_json(uint8_t index, const char *json_str, size_t length)
{
    if (index >= MAX_SCHEDULES || !json_str)
        return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_ParseWithLength(json_str, length);
    if (!root)
        return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(sched_mutex, portMAX_DELAY);
    schedule_profile_t *p = &system_schedules.profiles[index];

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "name")) && cJSON_IsString(item))
        strncpy(p->name, item->valuestring, SCHEDULE_NAME_LEN - 1);
    if ((item = cJSON_GetObjectItem(root, "is_active")) && cJSON_IsBool(item))
        p->is_active = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(root, "has_time")) && cJSON_IsBool(item))
        p->has_time_trigger = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(root, "hour")) && cJSON_IsNumber(item))
        p->start_hour = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "minute")) && cJSON_IsNumber(item))
        p->start_minute = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "interval")) && cJSON_IsNumber(item))
        p->interval_between_sec = item->valueint;

    cJSON *steps_arr = cJSON_GetObjectItem(root, "steps");
    if (steps_arr && cJSON_IsArray(steps_arr))
    {
        p->total_steps = cJSON_GetArraySize(steps_arr);
        if (p->total_steps > 8)
            p->total_steps = 8;

        for (int i = 0; i < p->total_steps; i++)
        {
            cJSON *step = cJSON_GetArrayItem(steps_arr, i);
            cJSON *z = cJSON_GetObjectItem(step, "zone");
            cJSON *t = cJSON_GetObjectItem(step, "time");
            if (z && t)
            {
                p->steps[i].zone_num = z->valueint;
                p->steps[i].runtime_seconds = t->valueint;
            }
        }
    }

    // Backup mutations instantly to local storage
    nvs_handle_t handle;
    if (nvs_open(NVS_SCHED_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_set_blob(handle, "config_blob", &system_schedules, sizeof(schedule_storage_t));
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Schedule Profile slot %d cleanly written to NVS partition.", index + 1);
    xSemaphoreGive(sched_mutex);
    cJSON_Delete(root);
    return ESP_OK;
}
