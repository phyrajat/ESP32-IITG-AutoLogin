#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_event.h"

// Events consumed by the Supervisor. Posted to the application loop, whose
// task is allowed to block on HTTPS requests.
ESP_EVENT_DECLARE_BASE(APP_EVENT);

enum : int32_t {
    APP_EVENT_NET_UP,        // Wi-Fi station has an IP address
    APP_EVENT_NET_DOWN,      // Wi-Fi link or IP address lost
    APP_EVENT_PROBE_DUE,     // time to check internet connectivity
    APP_EVENT_KEEPALIVE_DUE, // time to refresh the portal session
    APP_EVENT_RETRY_DUE,     // back-off expired
};

// State changes published by the Supervisor on the default loop.
// The event id is the new AppState.
ESP_EVENT_DECLARE_BASE(APP_STATE_EVENT);

enum class AppState : int32_t {
    WaitWifi,
    Probing,
    LoggingIn,
    Online,
    Backoff,
    ConfigError,
};

namespace app_events {

// Creates the application event loop and its task. Call once at startup.
esp_err_t init();

// The application loop, for registering handlers.
esp_event_loop_handle_t loop();

// Posts an APP_EVENT to the application loop. Safe from any task,
// including esp_timer callbacks.
esp_err_t post(int32_t eventId);

// Publishes a state change on the default loop.
esp_err_t publishState(AppState state);

const char *stateName(AppState state);

} // namespace app_events
