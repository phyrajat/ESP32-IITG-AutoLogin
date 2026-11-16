#pragma once

#include <string>

#include "esp_err.h"

struct Credentials {
    std::string wifiSsid;
    std::string wifiPasskey;
    std::string userId;
    std::string password;
};

// Reads and validates the credentials JSON file. Returns an error (and logs
// which field is wrong) instead of crashing on a missing or malformed file.
esp_err_t loadCredentials(const char *path, Credentials &out);
