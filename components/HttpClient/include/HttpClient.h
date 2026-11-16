#pragma once

#include <string>

#include "esp_err.h"

struct HttpResponse {
    esp_err_t err = ESP_FAIL; // transport result; ESP_OK means a response was received
    int status = 0;           // HTTP status code of the final response
    std::string body;

    bool ok() const { return err == ESP_OK; }
};

struct HttpOptions {
    bool followRedirects = true;
    int timeoutMs = 10000;
};

// Blocking requests. HTTPS servers are verified against the ESP-IDF
// certificate bundle. Call only from a task with a TLS-sized stack.
HttpResponse httpGet(const std::string &url, const HttpOptions &options = {});
HttpResponse httpPostForm(const std::string &url, const std::string &formBody,
                          const HttpOptions &options = {});
