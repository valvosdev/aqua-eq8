# Fetch global root directory
idf_build_get_property(project_dir PROJECT_DIR)

set(CERT_PATH "${project_dir}/../deployment/certs")
set(CA_FILE "${CERT_PATH}/valvos-dev-ca.crt")
set(CRT_FILE "${CERT_PATH}/client.crt")
set(KEY_FILE "${CERT_PATH}/client.key")

if(NOT EXISTS "${CA_FILE}" OR NOT EXISTS "${CRT_FILE}" OR NOT EXISTS "${KEY_FILE}")
    message(STATUS "🚨 MQTT Certs not found! Executing custom generation hook script...")
    
    execute_process(
        # Pass the dynamic target path parameter directly into the shell execution
        COMMAND bash "${project_dir}/../deployment/generate-cert-key.sh" "${CERT_PATH}"
        WORKING_DIRECTORY "${CERT_PATH}"
        RESULT_VARIABLE ACTION_RESULT
    )
    
    if(NOT ACTION_RESULT EQUAL 0)
        message(FATAL_ERROR "❌ Failed to execute custom certificate generation script!")
    endif()
endif()

idf_component_register(SRCS "device_mqtt.c"
                       INCLUDE_DIRS "include"
                        EMBED_TXTFILES "${project_dir}/../deployment/certs/valvos-dev-ca.crt"
                                      "${project_dir}/../deployment/certs/client.crt"
                                      "${project_dir}/../deployment/certs/client.key"
                       REQUIRES espressif__mqtt device_config zone_controller device_schedule lwip)