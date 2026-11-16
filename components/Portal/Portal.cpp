#include "Portal.h"

#include "esp_log.h"

#include "HttpClient.h"

namespace {

const char *TAG = "portal";

// A plain-HTTP endpoint with a known body. Behind the captive portal the
// request is intercepted and a redirect page to the portal comes back instead.
// (Google's generate_204 URL is no use here: the portal lets it through.)
const char *const kProbeUrl = "http://detectportal.firefox.com/success.txt";
const char *const kProbeExpectedBody = "success";
const char *const kPortalHost = "agnigarh.iitg.ac.in";

const std::string kPortalBase = "https://agnigarh.iitg.ac.in:1442";
const char *const kLoginRedirect = "https://agnigarh.iitg.ac.in/login?1";

bool isTokenChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

// Copies the run of [a-z0-9] characters that follows `marker` in `text`.
bool extractToken(const std::string &text, const char *marker, std::string &token)
{
    const std::string needle(marker);
    const size_t start = text.find(needle);
    if (start == std::string::npos) {
        return false;
    }
    size_t end = start + needle.size();
    const size_t first = end;
    while (end < text.size() && isTokenChar(text[end])) {
        ++end;
    }
    if (end == first) {
        return false;
    }
    token.assign(text, first, end - first);
    return true;
}

std::string urlEncode(const std::string &value)
{
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (const char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') ||
                                c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out.push_back(ch);
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

bool fetchMagic(std::string &magic)
{
    const HttpResponse page = httpGet(kPortalBase + "/login?1");
    if (!page.ok()) {
        return false;
    }
    if (!extractToken(page.body, "name=\"magic\" value=\"", magic)) {
        ESP_LOGW(TAG, "no magic token in login page (HTTP %d, %u bytes)", page.status,
                 static_cast<unsigned>(page.body.size()));
        return false;
    }
    return true;
}

} // namespace

namespace portal {

Probe probe()
{
    HttpOptions options;
    options.followRedirects = false;
    options.timeoutMs = 5000;

    const HttpResponse response = httpGet(kProbeUrl, options);
    if (!response.ok()) {
        return Probe::Error;
    }
    // The portal answers intercepted requests with a page that redirects to itself.
    if (response.body.find(kPortalHost) != std::string::npos) {
        ESP_LOGI(TAG, "probe intercepted by the portal: login needed");
        return Probe::Portal;
    }
    if (response.status == 200 && response.body.rfind(kProbeExpectedBody, 0) == 0) {
        return Probe::Internet;
    }
    ESP_LOGW(TAG, "unexpected probe response (HTTP %d, %u bytes)", response.status,
             static_cast<unsigned>(response.body.size()));
    return Probe::Error;
}

bool login(const std::string &userId, const std::string &password, std::string &keepaliveToken)
{
    std::string magic;
    if (!fetchMagic(magic)) {
        // A stale session can stop the portal from serving the login form.
        ESP_LOGI(TAG, "retrying login page after logout");
        logout();
        if (!fetchMagic(magic)) {
            return false;
        }
    }

    const std::string form = "4Tredir=" + urlEncode(kLoginRedirect) +
                             "&magic=" + magic +
                             "&username=" + urlEncode(userId) +
                             "&password=" + urlEncode(password);

    const HttpResponse response = httpPostForm(kPortalBase + "/", form);
    if (!response.ok()) {
        return false;
    }
    if (!extractToken(response.body, "keepalive?", keepaliveToken)) {
        ESP_LOGW(TAG, "login rejected (HTTP %d): check the IITG user id and password",
                 response.status);
        return false;
    }

    ESP_LOGI(TAG, "logged in");
    return true;
}

bool keepalive(const std::string &keepaliveToken)
{
    const HttpResponse response = httpGet(kPortalBase + "/keepalive?" + keepaliveToken);
    if (!response.ok() || response.status != 200) {
        ESP_LOGW(TAG, "keepalive failed (HTTP %d)", response.status);
        return false;
    }
    return true;
}

void logout()
{
    httpGet(kPortalBase + "/logout?1");
}

} // namespace portal
