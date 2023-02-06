#pragma once
#include <thread>
#include <string>
#include <chrono>
#include <fstream>

#include <esp_log.h>
#include <cJSON.h>

#include "WIFICXX.h"

class WIFIApp final
{
private:
	std::string messagetag = "[WiFi]";
	std::thread wifiappthread;
	void wifiapp();
public:
	void startWIFId();

	WIFI::Wifi::state_e wifiState = WIFI::Wifi::state_e::NOT_INITIALIZED;
	WIFI::Wifi Wifi;
};
