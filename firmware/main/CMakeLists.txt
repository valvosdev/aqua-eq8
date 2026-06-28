idf_component_register(SRCS "firmware.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                           shift_reg_74hc595 
                           adc_buttons 
                           zone_controller 
                           device_status 
                           device_config   
                           device_mqtt   
                           device_schedule  
                           nvs_flash 
                           network_provisioning 
                           esp_wifi
                           cjson     
                           lwip       
)