idf_component_register(
    SRCS "StatusLed.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES AppEvents esp_driver_gpio esp_event esp_timer)
