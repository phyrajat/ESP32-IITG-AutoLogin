#pragma once

#include "esp_err.h"

// Shows the application state on one LED by following APP_STATE_EVENT:
//   fast blink    joining Wi-Fi
//   slow blink    checking connectivity / logging in
//   solid         online
//   double blink  backing off after a failure, or bad credentials file
namespace status_led {

// Requires the default event loop. The GPIO comes from CONFIG_STATUS_LED_GPIO.
esp_err_t start();

} // namespace status_led
