#pragma once

#include "esp_err.h"

#include "Config.h"

// The application state machine. It runs entirely in the application event
// loop task: Wi-Fi/IP events and timer expiries arrive as APP_EVENTs, and the
// blocking portal requests are made from the handlers.
//
//   WAIT_WIFI --NET_UP--> PROBING --204--> ONLINE --PROBE_DUE--> PROBING
//                            |  \--error--> BACKOFF --RETRY_DUE--> PROBING
//                            \--portal--> LOGGING_IN --ok--> ONLINE
//                                              \--failed--> BACKOFF
//   any state --NET_DOWN--> WAIT_WIFI
namespace supervisor {

// Requires the default loop and app_events::init(). Call before Wi-Fi starts
// so that no network event is missed.
esp_err_t start(const Credentials &credentials);

} // namespace supervisor
