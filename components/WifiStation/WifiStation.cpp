#include "WifiStation.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

namespace {

const char *TAG = "wifi";

constexpr uint64_t kReconnectMinUs = 1 * 1000000ULL;
constexpr uint64_t kReconnectMaxUs = 30 * 1000000ULL;
// How long to wait for DHCP after associating before starting over.
constexpr uint64_t kNoIpTimeoutUs = 30 * 1000000ULL;

esp_netif_t *s_netif = nullptr;
esp_timer_handle_t s_reconnectTimer = nullptr;
esp_timer_handle_t s_noIpTimer = nullptr;
uint64_t s_reconnectDelayUs = kReconnectMinUs; // touched only from the default loop task

void scheduleReconnect()
{
    esp_timer_stop(s_reconnectTimer); // fails harmlessly if not running
    esp_timer_start_once(s_reconnectTimer, s_reconnectDelayUs);
    s_reconnectDelayUs = std::min(s_reconnectDelayUs * 2, kReconnectMaxUs);
}

void onReconnectTimer(void *)
{
    const esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        // No disconnect event will follow a failed call, so try again later.
        ESP_LOGW(TAG, "connect failed: %s", esp_err_to_name(err));
        esp_timer_start_once(s_reconnectTimer, kReconnectMaxUs);
    }
}

// One scan at boot, so a wrong SSID or a radio problem is obvious from the log.
void logVisibleNetworks()
{
    if (esp_wifi_scan_start(nullptr, true) != ESP_OK) {
        ESP_LOGW(TAG, "scan failed");
        return;
    }
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    ESP_LOGI(TAG, "%u networks visible:", static_cast<unsigned>(count));

    wifi_ap_record_t record;
    while (esp_wifi_scan_get_ap_record(&record) == ESP_OK) {
        ESP_LOGI(TAG, "  %-32s ch %2d  rssi %d", reinterpret_cast<const char *>(record.ssid),
                 static_cast<int>(record.primary), static_cast<int>(record.rssi));
    }
}

// Associated with the access point but DHCP never completed (seen after a
// router restart). Dropping the link restarts association and DHCP from scratch.
void onNoIpTimer(void *)
{
    esp_netif_dhcp_status_t dhcp = ESP_NETIF_DHCP_INIT;
    esp_netif_dhcpc_get_status(s_netif, &dhcp);
    ESP_LOGW(TAG, "no IP address %u s after associating (dhcp status %d), restarting the link",
             static_cast<unsigned>(kNoIpTimeoutUs / 1000000ULL), static_cast<int>(dhcp));
    esp_wifi_disconnect(); // raises STA_DISCONNECTED, which schedules the reconnect
}

void onWifiEvent(void *, esp_event_base_t, int32_t id, void *data)
{
    switch (id) {
    case WIFI_EVENT_STA_START:
        logVisibleNetworks();
        ESP_LOGI(TAG, "connecting");
        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "associated, waiting for an IP address");
        s_reconnectDelayUs = kReconnectMinUs; // the access point is there
        esp_timer_stop(s_noIpTimer);
        esp_timer_start_once(s_noIpTimer, kNoIpTimeoutUs);
        break;

    case WIFI_EVENT_STA_DISCONNECTED: {
        esp_timer_stop(s_noIpTimer);
        const auto *info = static_cast<const wifi_event_sta_disconnected_t *>(data);
        ESP_LOGW(TAG, "disconnected (reason %d), reconnecting in %" PRIu32 " ms",
                 static_cast<int>(info->reason),
                 static_cast<uint32_t>(s_reconnectDelayUs / 1000));
        scheduleReconnect();
        break;
    }

    default:
        break;
    }
}

void onIpEvent(void *, esp_event_base_t, int32_t id, void *data)
{
    if (id == IP_EVENT_STA_GOT_IP) {
        const auto *info = static_cast<const ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&info->ip_info.ip));
        esp_timer_stop(s_noIpTimer);
        s_reconnectDelayUs = kReconnectMinUs;
    } else if (id == IP_EVENT_STA_LOST_IP) {
        ESP_LOGW(TAG, "lost ip");
    }
}

} // namespace

namespace wifi_station {

esp_err_t start(const std::string &ssid, const std::string &passkey)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }
    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == nullptr) {
        return ESP_FAIL;
    }

    wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&initConfig);
    if (err != ESP_OK) {
        return err;
    }

    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = onReconnectTimer;
    timerArgs.name = "wifi_reconnect";
    err = esp_timer_create(&timerArgs, &s_reconnectTimer);
    if (err != ESP_OK) {
        return err;
    }

    esp_timer_create_args_t noIpArgs = {};
    noIpArgs.callback = onNoIpTimer;
    noIpArgs.name = "wifi_no_ip";
    err = esp_timer_create(&noIpArgs, &s_noIpTimer);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent,
                                              nullptr, nullptr);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, onIpEvent,
                                              nullptr, nullptr);
    if (err != ESP_OK) {
        return err;
    }

    // Zero-initialised, so shorter-than-maximum values stay null-terminated.
    wifi_config_t config = {};
    std::memcpy(config.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof(config.sta.ssid)));
    std::memcpy(config.sta.password, passkey.data(),
                std::min(passkey.size(), sizeof(config.sta.password)));
    config.sta.threshold.authmode = passkey.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (err != ESP_OK) {
        return err;
    }
    return esp_wifi_start(); // raises WIFI_EVENT_STA_START, which starts the first connect
}

} // namespace wifi_station
