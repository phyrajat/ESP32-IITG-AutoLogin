#pragma once

#include <string>

#include "esp_err.h"

namespace wifi_station {

// Brings up the Wi-Fi station and keeps it connected: it connects on start
// and reconnects with a back-off after every disconnect. Progress is reported
// through the standard WIFI_EVENT / IP_EVENT events on the default loop, which
// must already exist.
esp_err_t start(const std::string &ssid, const std::string &passkey);

} // namespace wifi_station
