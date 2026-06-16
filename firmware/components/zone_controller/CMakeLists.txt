idf_component_register(SRCS "zone_controller.c"
                      INCLUDE_DIRS "include"
                      PRIV_REQUIRES shift_reg_74hc595 adc_buttons freertos)