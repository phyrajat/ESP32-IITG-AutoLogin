//C++ specific
#include <thread>
#include <chrono>

//ESP32 specific
#include "nvs_flash.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <cJSON.h>

//Our own apps
#include "WIFIApp.h"
#include "IITGAutologin.h"

extern "C"
{
	void app_main(void);
}

void app_main(void)
{
	// Mounting the SPIFFS filesystem, our ids and passwords are stored there
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = false
	 };

	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK)
	{
		ESP_LOGI("[SPIFFS]", "Failed to mount");
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}else
	{
		ESP_LOGI("[SPIFFS]", "Mounted");
	}
	
	// Event loop and nvs for wifi/internet
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(nvs_flash_init());
	
	/*Launching our apps.*/
	
	//WiFi App
	WIFIApp App;
	App.startWIFId();
	
	//AutoLogin App
	AutoLogin login;
	login.startLogind();
	
	// Send main thread to sleep periodically or do some stuff
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
}
