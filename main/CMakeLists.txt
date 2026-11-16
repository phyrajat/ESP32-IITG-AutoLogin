idf_component_register(
    SRCS "main.cpp" "Supervisor.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES AppEvents Config WifiStation Portal StatusLed
                  spiffs nvs_flash esp_event esp_timer esp_wifi esp_netif esp_driver_gpio)

set(credentials_file "${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_image/credentials.json")
if(NOT EXISTS "${credentials_file}")
    message(FATAL_ERROR
        "spiffs_image/credentials.json is missing. "
        "Copy credentials.example.json to spiffs_image/credentials.json and fill it in.")
endif()

# Builds the SPIFFS image and flashes it together with the app on `idf.py flash`
spiffs_create_partition_image(credentials ../spiffs_image FLASH_IN_PROJECT)
