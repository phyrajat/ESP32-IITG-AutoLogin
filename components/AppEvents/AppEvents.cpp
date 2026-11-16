#include "AppEvents.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

ESP_EVENT_DEFINE_BASE(APP_EVENT);
ESP_EVENT_DEFINE_BASE(APP_STATE_EVENT);

namespace {

// HTTPS (TLS handshake) runs in this task, so it needs a generous stack.
constexpr uint32_t kAppTaskStack = 8192;
constexpr TickType_t kPostTimeout = pdMS_TO_TICKS(5000);

esp_event_loop_handle_t s_loop = nullptr;

} // namespace

namespace app_events {

esp_err_t init()
{
    if (s_loop != nullptr) {
        return ESP_OK;
    }

    esp_event_loop_args_t args = {};
    args.queue_size = 16;
    args.task_name = "app";
    args.task_priority = 5;
    args.task_stack_size = kAppTaskStack;
    args.task_core_id = tskNO_AFFINITY;

    return esp_event_loop_create(&args, &s_loop);
}

esp_event_loop_handle_t loop()
{
    return s_loop;
}

esp_err_t post(int32_t eventId)
{
    return esp_event_post_to(s_loop, APP_EVENT, eventId, nullptr, 0, kPostTimeout);
}

esp_err_t publishState(AppState state)
{
    return esp_event_post(APP_STATE_EVENT, static_cast<int32_t>(state), nullptr, 0, kPostTimeout);
}

const char *stateName(AppState state)
{
    switch (state) {
    case AppState::WaitWifi:    return "WAIT_WIFI";
    case AppState::Probing:     return "PROBING";
    case AppState::LoggingIn:   return "LOGGING_IN";
    case AppState::Online:      return "ONLINE";
    case AppState::Backoff:     return "BACKOFF";
    case AppState::ConfigError: return "CONFIG_ERROR";
    }
    return "?";
}

} // namespace app_events
