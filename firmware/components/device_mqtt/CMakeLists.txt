idf_component_register(SRCS "device_mqtt.c"
                       INCLUDE_DIRS "include"
                       REQUIRES espressif__mqtt device_config zone_controller device_schedule)