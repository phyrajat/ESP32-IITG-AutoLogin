# IITG AutoLogin on an esp32
My implementation of IITG AutoLogin on an esp32. I have used C++ wrappers of esp_wifi and esp_http_client components. This esp-tdf project is created using VisualGDB in Visual Studio but it can be cloned, compiled and flashed using just esp-idf.

The root certificate that the captive portal checks for is digicert and is contained in `components\HTTPCXX\certificates\digicert_global_root.pem`. Can be updated if it expires.

The wifi SSID, passkey and IITG login ID, passwords are supposed to be in the `spiffs_image\credentials.json` file.

# TODO: 
# 1. Implement a proper asynchronous version of this.
# 2. Blink lights on connect and login.

# Changes in menuconfig:
1. Increase cpu clock frequency
2. Increase flash size
3. Increase pthread stack size

# Changes in partition table:
1. Increase App/factory partition size to 2M
2. Add spiffs partition for storing credentials