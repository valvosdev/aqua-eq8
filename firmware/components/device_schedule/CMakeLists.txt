idf_component_register(SRCS "device_schedule.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash cjson zone_controller)