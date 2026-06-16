idf_component_register(SRCS "adc_buttons.c"
                      INCLUDE_DIRS "include"
                      PRIV_REQUIRES esp_adc esp_driver_gpio freertos)