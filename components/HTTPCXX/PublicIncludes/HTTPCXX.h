#pragma once

#include <functional>
#include <vector>

#include "esp_tls.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include <esp_http_client.h>

extern const char digicert_pem_start[] asm("_binary_digicert_global_root_pem_start");
extern const char digicert_pem_end[]   asm("_binary_digicert_global_root_pem_end");

class httpClientT
{
	//### CONSTRUCTOR/DESTRUCTOR ###//
public:
	//! Constructor for a client connecting to a single full URL
	httpClientT();

	~httpClientT()
	{}

	//! Performs HTTP GET request based on current configuration and stores response data into the given vector
	void Get(std::string urlIC, std::vector<char> & respBufIR);
	
	//! Performs HTTP POST request based on current configuration and stores response data into the given vector
	void Post(std::string urlIC, std::string payload, std::vector<char> & respBufIR);

	//! Performs HTTP request based on current configuration and executes the given
	//! call back for every client event.
	void Perform(std::function<void(esp_http_client_event_t *evt)> cb);

	//! Sets the http query which is appended to path <host><path>?<query>
	void SetQuery(const char * queryIC)
	{
		configM.query = queryIC;
	}
	
	//! Get the header
	void GetHeader(std::vector<char> & headerInfo)
	{
		
	}


protected:
	//! Routes c-style callback to c++ handler
	static esp_err_t HandleGlobalEvent(esp_http_client_event_t *evt);

	//! Handles http events
	esp_err_t HandleEvent(esp_http_client_event_t *evt);

	//! The configuration for the http client
	esp_http_client_config_t configM;

	//! Pointer to the http client
	esp_http_client_handle_t clientM;

	//! Pointer to callback for http event
	std::function<void(esp_http_client_event_t *evt)> * getCbMP;
	
	//!Temp string for url
	std::string url;
	
	esp_http_client_method_t method;
	
	std::string payload;

};