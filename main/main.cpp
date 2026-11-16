#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "AppEvents.h"
#include "Config.h"
#include "StatusLed.h"
#include "Supervisor.h"
#include "WifiStation.h"

namespace {

const char *TAG = "main";
const char *const kSpiffsLabel = "credentials";

// Reads the credentials from the SPIFFS partition, which is only needed at boot.
esp_err_t readCredentials(Credentials &credentials)
{
    esp_vfs_spiffs_conf_t spiffs = {};
    spiffs.base_path = "/spiffs";
    spiffs.partition_label = kSpiffsLabel;
    spiffs.max_files = 2;
    spiffs.format_if_mount_failed = false;

    esp_err_t err = esp_vfs_spiffs_register(&spiffs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot mount SPIFFS: %s", esp_err_to_name(err));
        return err;
    }

    err = loadCredentials("/spiffs/credentials.json", credentials);
    esp_vfs_spiffs_unregister(kSpiffsLabel);
    return err;
}

// Board-specific radio setup; a no-op unless enabled in menuconfig.
void initAntenna()
{
#ifdef CONFIG_XIAO_ESP32C6_ANTENNA_SWITCH
    constexpr gpio_num_t kRfSwitchEnable = GPIO_NUM_3;  // low = switch powered
    constexpr gpio_num_t kAntennaSelect = GPIO_NUM_14;  // low = on-board, high = external

    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << kRfSwitchEnable) | (1ULL << kAntennaSelect);
    io.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io));

    gpio_set_level(kRfSwitchEnable, 0);
#ifdef CONFIG_XIAO_ESP32C6_EXTERNAL_ANTENNA
    gpio_set_level(kAntennaSelect, 1);
#else
    gpio_set_level(kAntennaSelect, 0);
#endif
#endif
}

void initNvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

} // namespace

extern "C" void app_main(void)
{
    initNvs(); // Wi-Fi stores calibration data in NVS
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(app_events::init());
    ESP_ERROR_CHECK(status_led::start());

    Credentials credentials;
    if (readCredentials(credentials) != ESP_OK) {
        ESP_LOGE(TAG, "no usable credentials; fix spiffs_image/credentials.json and reflash");
        app_events::publishState(AppState::ConfigError);
        return;
    }

    initAntenna();
    ESP_ERROR_CHECK(supervisor::start(credentials));
    ESP_ERROR_CHECK(wifi_station::start(credentials.wifiSsid, credentials.wifiPasskey));

    // Nothing else to do here: everything from now on is driven by events.
}
