#include "Supervisor.h"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <string>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "AppEvents.h"
#include "Portal.h"

namespace {

const char *TAG = "supervisor";

constexpr uint64_t kSecondUs = 1000000ULL;
constexpr uint64_t kProbePeriodUs = 60 * kSecondUs;
constexpr uint64_t kKeepalivePeriodUs = 1199 * kSecondUs; // just under the portal's 20 minutes
constexpr uint64_t kKeepaliveSoonUs = 5 * kSecondUs;
constexpr uint64_t kBackoffMinUs = 5 * kSecondUs;
constexpr uint64_t kBackoffMaxUs = 300 * kSecondUs;

// Everything below is touched only from the application loop task.
Credentials s_credentials;
AppState s_state = AppState::WaitWifi;
std::string s_keepaliveToken;
uint64_t s_backoffUs = kBackoffMinUs;

esp_timer_handle_t s_probeTimer = nullptr;
esp_timer_handle_t s_keepaliveTimer = nullptr;
esp_timer_handle_t s_retryTimer = nullptr;

// ---- timers: each one only posts its event to the application loop ----

void onTimer(void *arg)
{
    app_events::post(static_cast<int32_t>(reinterpret_cast<intptr_t>(arg)));
}

esp_err_t createTimer(const char *name, int32_t eventId, esp_timer_handle_t *out)
{
    esp_timer_create_args_t args = {};
    args.callback = onTimer;
    args.arg = reinterpret_cast<void *>(static_cast<intptr_t>(eventId));
    args.name = name;
    return esp_timer_create(&args, out);
}

void arm(esp_timer_handle_t timer, uint64_t delayUs)
{
    esp_timer_stop(timer); // fails harmlessly if not running
    esp_timer_start_once(timer, delayUs);
}

void stopAllTimers()
{
    esp_timer_stop(s_probeTimer);
    esp_timer_stop(s_keepaliveTimer);
    esp_timer_stop(s_retryTimer);
}

// ---- state transitions ----

void setState(AppState state)
{
    if (state == s_state) {
        return;
    }
    ESP_LOGI(TAG, "%s -> %s", app_events::stateName(s_state), app_events::stateName(state));
    s_state = state;
    app_events::publishState(state);
}

void enterOnline()
{
    s_backoffUs = kBackoffMinUs;
    setState(AppState::Online);
    arm(s_probeTimer, kProbePeriodUs);

    // Coming back from a Wi-Fi drop with a session still held: refresh it soon.
    if (!s_keepaliveToken.empty() && !esp_timer_is_active(s_keepaliveTimer)) {
        arm(s_keepaliveTimer, kKeepaliveSoonUs);
    }
}

void enterBackoff()
{
    setState(AppState::Backoff);
    ESP_LOGW(TAG, "retrying in %" PRIu32 " s", static_cast<uint32_t>(s_backoffUs / kSecondUs));
    arm(s_retryTimer, s_backoffUs);
    s_backoffUs = std::min(s_backoffUs * 2, kBackoffMaxUs);
}

void login()
{
    setState(AppState::LoggingIn);

    if (!portal::login(s_credentials.userId, s_credentials.password, s_keepaliveToken)) {
        s_keepaliveToken.clear();
        enterBackoff();
        return;
    }

    portal::keepalive(s_keepaliveToken); // what a browser does right after login
    arm(s_keepaliveTimer, kKeepalivePeriodUs);
    enterOnline();
}

void probe()
{
    setState(AppState::Probing);

    switch (portal::probe()) {
    case portal::Probe::Internet:
        enterOnline();
        break;
    case portal::Probe::Portal:
        esp_timer_stop(s_keepaliveTimer);
        s_keepaliveToken.clear();
        login();
        break;
    case portal::Probe::Error:
        enterBackoff();
        break;
    }
}

// ---- application loop handler ----

void onAppEvent(void *, esp_event_base_t, int32_t id, void *)
{
    switch (id) {
    case APP_EVENT_NET_UP:
        if (s_state == AppState::WaitWifi) {
            s_backoffUs = kBackoffMinUs;
            probe();
        }
        break;

    case APP_EVENT_NET_DOWN:
        stopAllTimers();
        setState(AppState::WaitWifi);
        break;

    case APP_EVENT_PROBE_DUE:
        if (s_state == AppState::Online) {
            probe();
        }
        break;

    case APP_EVENT_RETRY_DUE:
        if (s_state == AppState::Backoff) {
            probe();
        }
        break;

    case APP_EVENT_KEEPALIVE_DUE:
        if (s_state != AppState::Online || s_keepaliveToken.empty()) {
            break;
        }
        if (portal::keepalive(s_keepaliveToken)) {
            arm(s_keepaliveTimer, kKeepalivePeriodUs);
        } else {
            s_keepaliveToken.clear();
            probe();
        }
        break;

    default:
        break;
    }
}

// ---- forwarders: run in the default loop task, so they only post ----

// Only link *changes* are forwarded. While the access point is away the driver
// raises a disconnect event for every failed attempt; forwarding each one could
// fill the application queue while its task is busy with HTTPS, and a NET_UP
// dropped for lack of room would leave the Supervisor waiting for ever.
bool s_linkUp = false; // touched only from the default loop task

void linkChanged(bool up)
{
    if (up == s_linkUp) {
        return;
    }
    s_linkUp = up;
    const esp_err_t err = app_events::post(up ? APP_EVENT_NET_UP : APP_EVENT_NET_DOWN);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "could not post link %s: %s", up ? "up" : "down", esp_err_to_name(err));
        s_linkUp = !up; // so the next event of this kind is tried again
    }
}

void onWifiEvent(void *, esp_event_base_t, int32_t id, void *)
{
    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        linkChanged(false);
    }
}

void onIpEvent(void *, esp_event_base_t, int32_t id, void *)
{
    if (id == IP_EVENT_STA_GOT_IP) {
        linkChanged(true);
    } else if (id == IP_EVENT_STA_LOST_IP) {
        linkChanged(false);
    }
}

} // namespace

namespace supervisor {

esp_err_t start(const Credentials &credentials)
{
    s_credentials = credentials;

    esp_err_t err = createTimer("probe", APP_EVENT_PROBE_DUE, &s_probeTimer);
    if (err == ESP_OK) {
        err = createTimer("keepalive", APP_EVENT_KEEPALIVE_DUE, &s_keepaliveTimer);
    }
    if (err == ESP_OK) {
        err = createTimer("retry", APP_EVENT_RETRY_DUE, &s_retryTimer);
    }
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register_with(app_events::loop(), APP_EVENT,
                                                       ESP_EVENT_ANY_ID, onAppEvent,
                                                       nullptr, nullptr);
    }
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                  onWifiEvent, nullptr, nullptr);
    }
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, onIpEvent,
                                                  nullptr, nullptr);
    }
    if (err != ESP_OK) {
        return err;
    }

    return app_events::publishState(s_state); // announce the initial WAIT_WIFI
}

} // namespace supervisor
