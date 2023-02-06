#Created by VisualGDB. Right-click on the component in Solution Explorer to edit properties using convenient GUI.

idf_component_register(
	SRCS "IITGAutologin.cpp"
	INCLUDE_DIRS PublicIncludes
	REQUIRES HTTPCXX esp_http_client nghttp esp-tls mbedtls json)
