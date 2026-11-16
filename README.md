# IITG AutoLogin for ESP32 (event-driven)

Keeps an ESP32 logged in to the IIT Guwahati captive portal (`agnigarh.iitg.ac.in`). It joins Wi-Fi, checks whether the portal is intercepting traffic, logs in only when it is, and keeps the session alive.

This is a from-scratch, event-driven rewrite for ESP-IDF v6.1. The original VisualGDB / ESP-IDF v4.4 version is in the git history.

## Setup

1. Install ESP-IDF v6.1 and open a shell with its environment exported.
2. Create a `spiffs_image` folder, copy `credentials.example.json` to `spiffs_image/credentials.json`, and fill in the Wi-Fi and IITG details. That file is git-ignored.
3. Build and flash. The credentials partition is flashed together with the app.

```bash
idf.py set-target esp32c6
```

Use the target that matches your chip (`esp32`, `esp32c6`, ...).

```bash
idf.py build flash monitor
```

After changing `spiffs_image/credentials.json`, run `idf.py flash` again.

Board settings are in `idf.py menuconfig` under **IITG AutoLogin**: the status LED pin and polarity, and the Seeed XIAO ESP32C6 antenna switch. `sdkconfig.defaults.esp32c6` presets them for the XIAO ESP32C6 (LED on GPIO 15, active low, on-board antenna).

The ESP32-C6 is 2.4 GHz only, so the SSID must be a 2.4 GHz network. The networks the board can see are logged once at boot.

The probe URL must be one the portal intercepts. Google's `generate_204` URL is not: the IITG portal lets it through even when logged out.

## Status LED

| Pattern | Meaning |
|---|---|
| Fast blink | Joining Wi-Fi |
| Slow blink | Checking connectivity or logging in |
| Solid | Online |
| Double blink | Backing off after a failure, or the credentials file is missing or invalid |

## How it works

```
default event loop (IDF)                 app event loop  (own task "app")
  WIFI_EVENT / IP_EVENT ──forwarders──▶  APP_EVENT_*  ──▶ Supervisor state machine
  APP_STATE_EVENT ◀──────────────────────────────────────  (does the blocking HTTPS)
        │                                     ▲
        ▼                                     │
    StatusLed                        esp_timer one-shots (probe / keepalive / retry)
```

- Nothing polls. Wi-Fi and IP events are forwarded to a second event loop, and every wait is an `esp_timer` one-shot that posts an event when it expires.
- The Supervisor runs in the second loop's task, so its handlers may block on HTTPS without stalling Wi-Fi event delivery.
- The Supervisor publishes each state change on the default loop; the LED only subscribes.

| State | What happens | Next |
|---|---|---|
| `WAIT_WIFI` | Timers stopped, waiting for an IP address | `PROBING` on `NET_UP` |
| `PROBING` | Plain-HTTP GET of a URL with a known body | expected body → `ONLINE`; portal redirect page → `LOGGING_IN`; anything else or no response → `BACKOFF` |
| `LOGGING_IN` | Fetch login page, post credentials, take the keepalive token | `ONLINE`, or `BACKOFF` on failure |
| `ONLINE` | Probe every 60 s, keepalive every 1199 s | `PROBING` when a probe is due or a keepalive fails |
| `BACKOFF` | Wait 5 s, doubling up to 5 min | `PROBING` |

Losing Wi-Fi from any state returns to `WAIT_WIFI`. Wi-Fi reconnects on its own with a 1 s to 30 s back-off.

## Layout

| Path | Role |
|---|---|
| `main/main.cpp` | Startup: NVS, event loops, LED, credentials, Supervisor, Wi-Fi |
| `main/Supervisor.cpp` | State machine and timers |
| `components/AppEvents` | Event bases and ids, the application loop |
| `components/Config` | Loads and validates `credentials.json` |
| `components/WifiStation` | Station bring-up and reconnect |
| `components/HttpClient` | Blocking GET/POST over `esp_http_client`, TLS via the certificate bundle |
| `components/Portal` | The IITG portal requests: probe, login, keepalive, logout |
| `components/StatusLed` | LED patterns |

## Tunables

- Probe URL and portal URLs: top of `components/Portal/Portal.cpp`.
- Probe, keepalive and back-off intervals: top of `main/Supervisor.cpp`.
