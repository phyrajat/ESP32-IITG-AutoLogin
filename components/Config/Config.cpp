#include "Config.h"

#include <cstdio>
#include <utility>

#include "cJSON.h"
#include "esp_log.h"

namespace {

const char *TAG = "config";
constexpr size_t kMaxFileSize = 4096;

bool readString(const cJSON *parent, const char *section, const char *key, std::string &out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) {
        ESP_LOGE(TAG, "\"%s\".\"%s\" is missing or not a string", section, key);
        return false;
    }
    out = item->valuestring;
    return true;
}

} // namespace

esp_err_t loadCredentials(const char *path, Credentials &out)
{
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGE(TAG, "cannot open %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    std::string text;
    char chunk[256];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        text.append(chunk, n);
        if (text.size() > kMaxFileSize) {
            break;
        }
    }
    fclose(file);

    if (text.size() > kMaxFileSize) {
        ESP_LOGE(TAG, "%s is larger than %u bytes", path, static_cast<unsigned>(kMaxFileSize));
        return ESP_ERR_INVALID_SIZE;
    }

    cJSON *root = cJSON_ParseWithLength(text.data(), text.size());
    if (root == nullptr) {
        ESP_LOGE(TAG, "%s is not valid JSON", path);
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    const cJSON *iitg = cJSON_GetObjectItemCaseSensitive(root, "iitg");

    Credentials parsed;
    // Evaluate all four so every problem is logged in one boot.
    bool ok = readString(wifi, "wifi", "ssid", parsed.wifiSsid);
    ok = readString(wifi, "wifi", "passkey", parsed.wifiPasskey) && ok;
    ok = readString(iitg, "iitg", "user-id", parsed.userId) && ok;
    ok = readString(iitg, "iitg", "password", parsed.password) && ok;

    cJSON_Delete(root);

    if (!ok) {
        return ESP_ERR_INVALID_ARG;
    }
    if (parsed.wifiSsid.empty() || parsed.wifiSsid.size() > 32) {
        ESP_LOGE(TAG, "wifi.ssid must be 1 to 32 bytes");
        return ESP_ERR_INVALID_ARG;
    }
    if (parsed.wifiPasskey.size() > 64) {
        ESP_LOGE(TAG, "wifi.passkey must be at most 64 bytes");
        return ESP_ERR_INVALID_ARG;
    }
    if (parsed.userId.empty() || parsed.password.empty()) {
        ESP_LOGE(TAG, "iitg.user-id and iitg.password must not be empty");
        return ESP_ERR_INVALID_ARG;
    }

    out = std::move(parsed);
    return ESP_OK;
}
