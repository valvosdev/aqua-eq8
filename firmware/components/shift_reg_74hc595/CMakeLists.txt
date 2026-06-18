idf_component_register(SRCS "shift_reg_74hc595.c"
                      INCLUDE_DIRS "include"
                      REQUIRES esp_driver_gpio
                      PRIV_REQUIRES freertos)