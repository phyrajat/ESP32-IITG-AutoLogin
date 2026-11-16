#include "StatusLed.h"

#include <atomic>
#include <cstdint>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "AppEvents.h"

namespace {

const gpio_num_t kLedGpio = static_cast<gpio_num_t>(CONFIG_STATUS_LED_GPIO);
#ifdef CONFIG_STATUS_LED_ACTIVE_LOW
constexpr bool kActiveLow = true;
#else
constexpr bool kActiveLow = false;
#endif

// A pattern is 20 steps of 100 ms (a 2 s cycle); bit n set = LED on in step n.
constexpr uint32_t kSteps = 20;
constexpr uint64_t kStepUs = 100 * 1000ULL;

constexpr uint32_t kFastBlink   = 0x55555; // 100 ms on / 100 ms off
constexpr uint32_t kSlowBlink   = 0x07C1F; // 500 ms on / 500 ms off
constexpr uint32_t kSolid       = 0xFFFFF;
constexpr uint32_t kDoubleBlink = 0x00033; // two 200 ms flashes, then dark

std::atomic<uint32_t> s_pattern{kFastBlink};
uint32_t s_step = 0; // touched only from the timer callback

uint32_t patternFor(AppState state)
{
    switch (state) {
    case AppState::WaitWifi:    return kFastBlink;
    case AppState::Probing:     return kSlowBlink;
    case AppState::LoggingIn:   return kSlowBlink;
    case AppState::Online:      return kSolid;
    case AppState::Backoff:     return kDoubleBlink;
    case AppState::ConfigError: return kDoubleBlink;
    }
    return kDoubleBlink;
}

void onTick(void *)
{
    const bool on = (s_pattern.load() >> s_step) & 1U;
    gpio_set_level(kLedGpio, on != kActiveLow ? 1 : 0);
    s_step = (s_step + 1) % kSteps;
}

void onStateEvent(void *, esp_event_base_t, int32_t id, void *)
{
    s_pattern.store(patternFor(static_cast<AppState>(id)));
}

} // namespace

namespace status_led {

esp_err_t start()
{
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << CONFIG_STATUS_LED_GPIO;
    io.mode = GPIO_MODE_OUTPUT;
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(APP_STATE_EVENT, ESP_EVENT_ANY_ID, onStateEvent,
                                              nullptr, nullptr);
    if (err != ESP_OK) {
        return err;
    }

    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = onTick;
    timerArgs.name = "status_led";
    esp_timer_handle_t timer = nullptr;
    err = esp_timer_create(&timerArgs, &timer);
    if (err != ESP_OK) {
        return err;
    }
    return esp_timer_start_periodic(timer, kStepUs);
}

} // namespace status_led
