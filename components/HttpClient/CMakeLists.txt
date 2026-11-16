idf_component_register(
    SRCS "HttpClient.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_client mbedtls)
