#pragma once

// Wi-Fi network used by both the ESP32 and the monitored server.
#define WIFI_SSID "home_48"
#define WIFI_PASSWORD "04052018"

// Use the server's LAN IP, not localhost. Example:
// http://192.168.1.50:8000/stats
#define SERVER_STATS_URL "http://192.168.0.201:8020/stats"

// Common ESP32 VSPI wiring for ST7735 displays.
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4
#define TFT_SCLK  18
#define TFT_MOSI  23
#define TFT_BL    15

// Set to 128 for a 1.44" 128x128 panel or 160 for a 1.8" 128x160 panel.
#define TFT_PANEL_HEIGHT 160
#define TFT_ROTATION 1

#define POLL_INTERVAL_MS 5000UL
#define HTTP_TIMEOUT_MS 3000
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
