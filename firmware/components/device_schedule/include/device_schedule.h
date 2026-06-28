#ifndef DEVICE_SCHEDULE_H
#define DEVICE_SCHEDULE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "zone_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SCHEDULES   4
#define SCHEDULE_NAME_LEN 32

typedef struct {
    uint8_t zone_num;            // 1 to 8
    uint32_t runtime_seconds;    // Default: 420 (7 minutes)
} schedule_step_t;

typedef struct {
    char name[SCHEDULE_NAME_LEN];
    bool is_active;              // Enabled or disabled
    bool has_time_trigger;       // true if start hour/minute are valid
    uint8_t start_hour;          // 0-23
    uint8_t start_minute;        // 0-59
    uint32_t interval_between_sec; // Default: 20 seconds
    uint8_t total_steps;         // Number of valid steps populated
    schedule_step_t steps[8];    // Sequence array for zones
} schedule_profile_t;

typedef struct {
    schedule_profile_t profiles[MAX_SCHEDULES];
} schedule_storage_t;

/**
 * @brief Initialises the background scheduler thread engine and NVS defaults.
 */
esp_err_t device_schedule_init(zone_controller_handle_t zone_engine);

/**
 * @brief Starts execution of a specific schedule profile by its array index.
 * @param index 0 to (MAX_SCHEDULES - 1)
 */
esp_err_t device_schedule_start_by_index(uint8_t index);

/**
 * @brief Stops any currently running background schedule sequence instantly.
 */
void device_schedule_stop_current(void);

/**
 * @brief Safely parses a JSON packet string modifying a specific schedule array slot.
 */
esp_err_t device_schedule_update_from_json(uint8_t index, const char *json_str, size_t length);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_SCHEDULE_H
