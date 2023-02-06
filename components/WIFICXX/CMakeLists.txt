#Created by VisualGDB. Right-click on the component in Solution Explorer to edit properties using convenient GUI.

idf_component_register(
	SRCS "WIFICXX.cpp"
	INCLUDE_DIRS PublicIncludes
	REQUIRES esp_event esp_wifi)
