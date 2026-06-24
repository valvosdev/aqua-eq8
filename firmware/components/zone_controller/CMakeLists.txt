idf_component_register(SRCS "zone_controller.c"
                       INCLUDE_DIRS "include"
                       REQUIRES shift_reg_74hc595 adc_buttons 
                       PRIV_REQUIRES freertos                 
)