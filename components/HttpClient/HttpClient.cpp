#include "HttpClient.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

namespace {

const char *TAG = "http";

// Portal pages are a few KB; cap the body so a rogue response cannot exhaust the heap.
constexpr size_t kMaxBody = 32 * 1024;

esp_err_t onHttpEvent(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0) {
        auto *response = static_cast<HttpResponse *>(evt->user_data);
        const size_t room = kMaxBody - response->body.size();
        const size_t len = static_cast<size_t>(evt->data_len) < room
                               ? static_cast<size_t>(evt->data_len) : room;
        response->body.append(static_cast<const char *>(evt->data), len);
    }
    return ESP_OK;
}

HttpResponse perform(const std::string &url, esp_http_client_method_t method,
                     const std::string *formBody, const HttpOptions &options)
{
    HttpResponse response;

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = method;
    config.timeout_ms = options.timeoutMs;
    config.disable_auto_redirect = !options.followRedirects;
    config.event_handler = onHttpEvent;
    config.user_data = &response;
    config.buffer_size = 2048; // portal redirects carry long Location headers
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "client init failed for %s", url.c_str());
        response.err = ESP_FAIL;
        return response;
    }

    if (formBody != nullptr) {
        esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
        esp_http_client_set_post_field(client, formBody->data(), static_cast<int>(formBody->size()));
    }

    response.err = esp_http_client_perform(client);
    if (response.err == ESP_OK) {
        response.status = esp_http_client_get_status_code(client);
    } else {
        ESP_LOGW(TAG, "%s failed: %s", url.c_str(), esp_err_to_name(response.err));
    }

    esp_http_client_cleanup(client);
    return response;
}

} // namespace

HttpResponse httpGet(const std::string &url, const HttpOptions &options)
{
    return perform(url, HTTP_METHOD_GET, nullptr, options);
}

HttpResponse httpPostForm(const std::string &url, const std::string &formBody,
                          const HttpOptions &options)
{
    return perform(url, HTTP_METHOD_POST, &formBody, options);
}
