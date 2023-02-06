#Created by VisualGDB. Right-click on the component in Solution Explorer to edit properties using convenient GUI.

idf_component_register(
	SRCS "HTTPCXX.cpp"
	INCLUDE_DIRS PublicIncludes
	REQUIRES esp_http_client nghttp esp-tls mbedtls
	EMBED_TXTFILES "certificates/digicert_global_root.pem")
