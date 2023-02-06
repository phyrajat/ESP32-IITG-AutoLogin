#pragma once
#include <thread>
#include <string>
#include <fstream>
#include <regex.h>

#include <esp_log.h>
#include <cJSON.h>

#include "HTTPCXX.h"


class AutoLogin final
{
	std::string messagetag = "[AutoLogin]";
	httpClientT client;
	std::string id, password;
	std::thread loginmanager;
	
	void logout()
	{
		ESP_LOGI(messagetag.c_str(), "Logging Out...");
		std::vector<char> response;
		client.Get("https://agnigarh.iitg.ac.in:1442/logout?1", response);
	}
	
	bool getMagic()
	{
		logout();
		ESP_LOGI(messagetag.c_str(), "Requesting Login Page...");
		std::vector<char> response;
		client.Get("https://agnigarh.iitg.ac.in:1442/login?1", response);
		
		regex_t regex;
		int reti;
		reti = regcomp(&regex, "name=\"magic\" value=\"([a-z0-9]+)", REG_EXTENDED);
		if (reti) {
			ESP_LOGI(messagetag.c_str(), "Could not compile regex");
			return false;
		}
		regmatch_t matches[2];
		reti = regexec(&regex, response.data(), 2, matches, 0);
		
		if (!reti) {
			magic = std::string(&response[matches[1].rm_so], &response[matches[1].rm_eo]);
			ESP_LOGI(messagetag.c_str(), "Magic found: %s", magic.c_str());
			regfree(&regex);
			return true;
		}
		else if (reti == REG_NOMATCH) {
			ESP_LOGI(messagetag.c_str(), "Magic not found");
			regfree(&regex);
			std::string str_response = std::string(response.begin(), response.end());
			ESP_LOGI(messagetag.c_str(), "Response Body:\n%s", str_response.c_str());
			return false;
		}
		else {
			regfree(&regex);
			ESP_LOGI(messagetag.c_str(), "Regex Error");
			std::string str_response = std::string(response.begin(), response.end());
			ESP_LOGI(messagetag.c_str(), "Response Body:\n%s", str_response.c_str());
			esp_restart();
		}
	}
	
	bool getKeepAlive()
	{
		ESP_LOGI(messagetag.c_str(), "Logging in...");
		std::vector<char> response;
		client.Post("https://agnigarh.iitg.ac.in:1442/", "4Tredir=https://agnigarh.iitg.ac.in/login?1&magic=" + magic + "&username=" + id + "&password=" + password, response);
		
		regex_t regex;
		int reti;
		reti = regcomp(&regex, "keepalive\\?([a-z0-9]+)", REG_EXTENDED);
		if (reti) {
			ESP_LOGI(messagetag.c_str(), "Could not compile regex");
			return false;
		}
		regmatch_t matches[2];
		reti = regexec(&regex, response.data(), 2, matches, 0);
		
		if (!reti) {
			keepalive = std::string(&response[matches[1].rm_so], &response[matches[1].rm_eo]);
			ESP_LOGI(messagetag.c_str(), "Keepalive found: %s", keepalive.c_str());
			regfree(&regex);
			return true;
		}
		else if (reti == REG_NOMATCH) {
			ESP_LOGI(messagetag.c_str(), "Keepalive not found");
			regfree(&regex);
			std::string str_response = std::string(response.begin(), response.end());
			ESP_LOGI(messagetag.c_str(), "Response Body:\n%s", str_response.c_str());
			return false;
		}
		else {
			regfree(&regex);
			ESP_LOGI(messagetag.c_str(), "Regex Error");
			std::string str_response = std::string(response.begin(), response.end());
			ESP_LOGI(messagetag.c_str(), "Response Body:\n%s", str_response.c_str());
			esp_restart();
		}
	}
	
	bool keepAlive()
	{
		ESP_LOGI(messagetag.c_str(), "Keepalive Redirect...");
		std::vector<char> response;
		client.Get("https://agnigarh.iitg.ac.in:1442/keepalive?"+keepalive, response);
		return true;
	}
	
	void loginApp()
	{
		while (true)
		{
			if (!getMagic())
				continue;
			if (!getKeepAlive())
				continue;
			keepAlive();
			std::this_thread::sleep_for(std::chrono::seconds(1200-1));
		}
		
		
		ESP_LOGI(messagetag.c_str(), "Exiting Autologin Manager...");
	}
public:
	void startLogind()
	{
		std::ifstream configfile("/spiffs/credentials.json");
		std::string config((std::istreambuf_iterator<char>(configfile)), 
			std::istreambuf_iterator<char>());
	
		cJSON* configdata = NULL;
		cJSON* logindata = NULL;
		cJSON* loginid = NULL;
		cJSON* loginpwd = NULL;
		cJSON* keepalive = NULL;
	
		configdata = cJSON_Parse(config.data());
	
		logindata = cJSON_GetObjectItem(configdata, "iitg");
	
		loginid = cJSON_GetObjectItem(logindata, "user-id");
		loginpwd = cJSON_GetObjectItem(logindata, "password");
		keepalive = cJSON_GetObjectItem(logindata, "last-keepalive");
		
		id = loginid->valuestring;
		password = loginpwd->valuestring;
		this->keepalive = keepalive->valuestring;
	
		configfile.close();
		
		loginmanager = std::thread(&AutoLogin::loginApp, this);
	}
	
	std::string magic, keepalive;
};