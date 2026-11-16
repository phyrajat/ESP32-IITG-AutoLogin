idf_component_register(
    SRCS "WifiStation.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event esp_timer)
