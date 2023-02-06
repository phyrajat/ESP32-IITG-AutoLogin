#Created by VisualGDB. Right-click on the component in Solution Explorer to edit properties using convenient GUI.

idf_component_register(
	SRCS "WIFIApp.cpp"
	INCLUDE_DIRS PublicIncludes
	REQUIRES json WIFICXX)
