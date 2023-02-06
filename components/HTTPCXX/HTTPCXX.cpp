#include <cstring>
#include "esp_log.h"

#include <HTTPCXX.h>

static const char * TAG = "httpClientT";

httpClientT::httpClientT() :
configM {
	.cert_pem = digicert_pem_start,
	.event_handler = httpClientT::HandleGlobalEvent,
	.user_data = this
}, clientM(nullptr), getCbMP(nullptr) { 
	
}

esp_err_t httpClientT::HandleGlobalEvent(esp_http_client_event_t *evt)
{
	auto inst = static_cast<httpClientT*>(evt->user_data);
	return inst->HandleEvent(evt);

} 

esp_err_t httpClientT::HandleEvent(esp_http_client_event_t *evt)
{
	switch (evt->event_id) {
	case HTTP_EVENT_ERROR:
		ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
		break;
	case HTTP_EVENT_ON_CONNECTED:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
		break;
	case HTTP_EVENT_HEADER_SENT:
		ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
		break;
	case HTTP_EVENT_ON_HEADER:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
		printf("%.*s", evt->data_len, (char*)evt->data);
		break;
	case HTTP_EVENT_ON_DATA:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
		if (!esp_http_client_is_chunked_response(evt->client))
		{
			//printf("%.*s", evt->data_len, (char*)evt->data);
		}

		break;
	case HTTP_EVENT_ON_FINISH:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
		break;
	case HTTP_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
		break;
	}

	if (getCbMP != nullptr)
	{
		(*getCbMP)(evt);
	}

	return ESP_OK;

} 

void httpClientT::Get(std::string urlIC, std::vector<char> & respBufIR)
{
	//Update the temporary url
	url = urlIC;
	configM.url = url.data();
	
	//Update the method
	method = HTTP_METHOD_GET;
	
	// Create call back to process client response and store in buffer
	auto saveResponseCb = [&respBufIR](esp_http_client_event_t *evt)
	{
		if (evt->event_id == HTTP_EVENT_ON_DATA)
		{
			// Resize response buffer to hold data and copy in response
			const std::size_t curSizeC = respBufIR.size();
			const std::size_t dataLenC = evt->data_len;
			respBufIR.resize(curSizeC + dataLenC);
			memcpy(&respBufIR[curSizeC], evt->data, dataLenC);
		}
	};

	// Perform the query using the above lambda to store the response
	Perform(saveResponseCb);

} 

void httpClientT::Post(std::string urlIC, std::string payload, std::vector<char> & respBufIR)
{
	//Update the temporary url
	url = urlIC;
	configM.url = url.data();
	
	//Update payload
	this->payload = payload;
	
	//Update the method
	method = HTTP_METHOD_POST;
	
	// Create call back to process client response and store in buffer
	auto saveResponseCb = [&respBufIR](esp_http_client_event_t *evt)
	{
		if (evt->event_id == HTTP_EVENT_ON_DATA)
		{
			// Resize response buffer to hold data and copy in response
			const std::size_t curSizeC = respBufIR.size();
			const std::size_t dataLenC = evt->data_len;
			respBufIR.resize(curSizeC + dataLenC);
			memcpy(&respBufIR[curSizeC], evt->data, dataLenC);
		}
	};

	// Perform the query using the above lambda to store the response
	Perform(saveResponseCb);

} 

void httpClientT::Perform(std::function<void(esp_http_client_event_t *evt)> cb)
{
	getCbMP = &cb;
	clientM = esp_http_client_init(&configM);
	
	switch (method)
	{
	case HTTP_METHOD_GET:
		esp_http_client_set_method(clientM, HTTP_METHOD_GET);
		break;
	case HTTP_METHOD_POST:
		esp_http_client_set_method(clientM, HTTP_METHOD_POST);
		esp_http_client_set_post_field(clientM, payload.data(), payload.length());
		break;
	default:
		break;
	}
	
	
	esp_err_t err = esp_http_client_perform(clientM);

	if (err == ESP_OK)
	{
		ESP_LOGI(TAG,
			"Status = %d, content_length = %d",
			esp_http_client_get_status_code(clientM),
			esp_http_client_get_content_length(clientM));
	}
	esp_http_client_cleanup(clientM);

	getCbMP = nullptr;

}