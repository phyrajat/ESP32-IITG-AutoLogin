//TODO: add code for your component here
#include "WIFIApp.h"


void WIFIApp::wifiapp()
{
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		wifiState = Wifi.GetState();

		switch (wifiState)
		{
		case WIFI::Wifi::state_e::READY_TO_CONNECT:
			ESP_LOGI(messagetag.c_str(), "READY_TO_CONNECT");
			Wifi.Begin();
			break;
		case WIFI::Wifi::state_e::DISCONNECTED:
			ESP_LOGI(messagetag.c_str(), "DISCONNECTED");
			Wifi.Begin();
			break;
		case WIFI::Wifi::state_e::CONNECTING:
			ESP_LOGI(messagetag.c_str(), "CONNECTING");
			break;
		case WIFI::Wifi::state_e::WAITING_FOR_IP:
			ESP_LOGI(messagetag.c_str(), "WAITING_FOR_IP");
			break;
		case WIFI::Wifi::state_e::ERROR:
			ESP_LOGI(messagetag.c_str(), "ERROR");
			break;
		case WIFI::Wifi::state_e::CONNECTED:
			ESP_LOGI(messagetag.c_str(), "CONNECTED");
			break;
		case WIFI::Wifi::state_e::NOT_INITIALIZED:
			ESP_LOGI(messagetag.c_str(), "NOT_INITIALIZED");
			break;
		case WIFI::Wifi::state_e::INITIALIZED:
			ESP_LOGI(messagetag.c_str(), "INITIALIZED");
			break;
		}
	}
}

void WIFIApp::startWIFId()
{
	std::ifstream configfile("/spiffs/credentials.json");
	std::string config((std::istreambuf_iterator<char>(configfile)), 
		std::istreambuf_iterator<char>());
	
	cJSON* configdata = NULL;
	cJSON* wifidata = NULL;
	cJSON* wifiSSID = NULL;
	cJSON* wifiPSK = NULL;
	
	configdata = cJSON_Parse(config.data());
	
	wifidata = cJSON_GetObjectItem(configdata, "wifi");
	
	wifiSSID = cJSON_GetObjectItem(wifidata, "ssid");
	wifiPSK = cJSON_GetObjectItem(wifidata, "passkey");
	
	configfile.close();
	
	Wifi.SetCredentials(wifiSSID->valuestring, wifiPSK->valuestring);
	Wifi.Init();
	
	wifiappthread = std::thread(&WIFIApp::wifiapp, this);
	
	while (wifiState != WIFI::Wifi::state_e::CONNECTED)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}