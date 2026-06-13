#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
WebServer webServer(80);

constexpr uint16_t COLOR_ORANGE = 0xFD20;
constexpr uint16_t COLOR_DARK_GREY = 0x7BEF;

struct ServerStats {
  float cpu = 0.0F;
  float memory = 0.0F;
  float disk = 0.0F;
  uint32_t timestamp = 0;
  bool valid = false;
};

ServerStats stats;
unsigned long lastPollMillis = 0;
String lastError;

uint16_t colorForPercent(float percent) {
  if (percent >= 85.0F) {
    return ST77XX_RED;
  }
  if (percent >= 65.0F) {
    return COLOR_ORANGE;
  }
  return ST77XX_GREEN;
}

float clampPercent(float percent) {
  if (percent < 0.0F) {
    return 0.0F;
  }
  if (percent > 100.0F) {
    return 100.0F;
  }
  return percent;
}

void drawCenteredText(const String &text, int16_t y, uint16_t color,
                      uint8_t size) {
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  tft.setTextSize(size);
  tft.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  tft.setTextColor(color);
  tft.setCursor((tft.width() - width) / 2, y);
  tft.print(text);
}

void drawProgressBar(int16_t y, const char *label, float rawPercent) {
  const float percent = clampPercent(rawPercent);
  const int16_t margin = 7;
  const int16_t barWidth = tft.width() - margin * 2;
  const int16_t barHeight = tft.height() <= 128 ? 12 : 14;
  const int16_t fillWidth =
      static_cast<int16_t>((barWidth - 4) * percent / 100.0F);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(margin, y);
  tft.print(label);

  String value = String(percent, 1) + "%";
  int16_t x1;
  int16_t y1;
  uint16_t textWidth;
  uint16_t textHeight;
  tft.getTextBounds(value, 0, y, &x1, &y1, &textWidth, &textHeight);
  tft.setCursor(tft.width() - margin - textWidth, y);
  tft.print(value);

  const int16_t barY = y + 10;
  tft.drawRoundRect(margin, barY, barWidth, barHeight, 3, ST77XX_WHITE);
  tft.fillRoundRect(margin + 2, barY + 2, barWidth - 4, barHeight - 4, 2,
                    ST77XX_BLACK);
  if (fillWidth > 0) {
    tft.fillRoundRect(margin + 2, barY + 2, fillWidth, barHeight - 4, 2,
                      colorForPercent(percent));
  }
}

void drawStatusScreen() {
  tft.fillScreen(ST77XX_BLACK);
  drawCenteredText("SERVER MONITOR", 5, ST77XX_CYAN, 1);
  tft.drawFastHLine(5, 16, tft.width() - 10, ST77XX_BLUE);

  if (!stats.valid) {
    drawCenteredText(WiFi.status() == WL_CONNECTED ? "NO SERVER DATA"
                                                   : "CONNECTING WIFI",
                     tft.height() / 2 - 12, ST77XX_YELLOW, 1);
    if (lastError.length() > 0) {
      drawCenteredText(lastError.substring(0, 20), tft.height() / 2 + 5,
                       ST77XX_RED, 1);
    }
    return;
  }

  const int16_t firstY = tft.height() <= 128 ? 24 : 28;
  const int16_t spacing = tft.height() <= 128 ? 29 : 34;
  drawProgressBar(firstY, "CPU", stats.cpu);
  drawProgressBar(firstY + spacing, "RAM", stats.memory);
  drawProgressBar(firstY + spacing * 2, "DISK", stats.disk);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_DARK_GREY, ST77XX_BLACK);
  tft.setCursor(5, tft.height() - 9);
  tft.print(WiFi.localIP());
}

bool fetchServerStats() {
  if (WiFi.status() != WL_CONNECTED) {
    lastError = "WiFi offline";
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(client, SERVER_STATS_URL)) {
    lastError = "Bad server URL";
    return false;
  }

  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    lastError = "HTTP " + String(statusCode);
    http.end();
    return false;
  }

  StaticJsonDocument<768> document;
  const DeserializationError jsonError =
      deserializeJson(document, http.getStream());
  http.end();

  if (jsonError) {
    lastError = "Invalid JSON";
    return false;
  }

  if (!document["cpu"]["usage_percent"].is<float>() ||
      !document["memory"]["usage_percent"].is<float>() ||
      !document["disk"]["usage_percent"].is<float>()) {
    lastError = "Missing fields";
    return false;
  }

  stats.cpu = document["cpu"]["usage_percent"].as<float>();
  stats.memory = document["memory"]["usage_percent"].as<float>();
  stats.disk = document["disk"]["usage_percent"].as<float>();
  stats.timestamp = document["timestamp"] | 0;
  stats.valid = true;
  lastError = "";
  return true;
}

String jsonStatus() {
  StaticJsonDocument<384> document;
  document["connected"] = WiFi.status() == WL_CONNECTED;
  document["server_data_valid"] = stats.valid;
  document["cpu_percent"] = stats.cpu;
  document["memory_percent"] = stats.memory;
  document["disk_percent"] = stats.disk;
  document["server_timestamp"] = stats.timestamp;
  document["error"] = lastError;

  String response;
  serializeJson(document, response);
  return response;
}

void handleDashboard() {
  static const char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="uk">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Server Monitor</title>
  <style>
    body{font-family:Arial,sans-serif;background:#10151b;color:#eef;margin:0;padding:24px}
    main{max-width:520px;margin:auto}h1{font-size:24px}
    .row{margin:22px 0}.label{display:flex;justify-content:space-between;margin-bottom:7px}
    .bar{height:18px;background:#29313a;border-radius:9px;overflow:hidden}
    .fill{height:100%;width:0;background:#27c46b;transition:width .4s}
    #error{color:#ff6b6b;min-height:20px}
  </style>
</head>
<body><main>
  <h1>Server Monitor</h1><div id="error"></div>
  <div class="row"><div class="label"><span>CPU</span><b id="cpuText">--</b></div><div class="bar"><div id="cpu" class="fill"></div></div></div>
  <div class="row"><div class="label"><span>RAM</span><b id="memoryText">--</b></div><div class="bar"><div id="memory" class="fill"></div></div></div>
  <div class="row"><div class="label"><span>Диск</span><b id="diskText">--</b></div><div class="bar"><div id="disk" class="fill"></div></div></div>
</main><script>
function color(v){return v>=85?'#ef4444':v>=65?'#f59e0b':'#27c46b'}
function setBar(id,v){v=Math.max(0,Math.min(100,Number(v)||0));document.getElementById(id).style.width=v+'%';document.getElementById(id).style.background=color(v);document.getElementById(id+'Text').textContent=v.toFixed(1)+'%'}
async function update(){try{const r=await fetch('/api/status');const s=await r.json();setBar('cpu',s.cpu_percent);setBar('memory',s.memory_percent);setBar('disk',s.disk_percent);document.getElementById('error').textContent=s.error||''}catch(e){document.getElementById('error').textContent='ESP32 не відповідає'}}
update();setInterval(update,5000);
</script></body></html>
)HTML";
  webServer.send_P(200, "text/html; charset=utf-8", PAGE);
}

void configureWebServer() {
  webServer.on("/", HTTP_GET, handleDashboard);
  webServer.on("/api/status", HTTP_GET, []() {
    webServer.send(200, "application/json; charset=utf-8", jsonStatus());
  });
  webServer.onNotFound([]() {
    webServer.send(404, "application/json", "{\"error\":\"Not found\"}");
  });
  webServer.begin();
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }
}

void initializeDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
#if TFT_PANEL_HEIGHT == 128
  tft.initR(INITR_144GREENTAB);
#elif TFT_PANEL_HEIGHT == 160
  tft.initR(INITR_BLACKTAB);
#else
#error "TFT_PANEL_HEIGHT must be 128 or 160"
#endif
  tft.setRotation(TFT_ROTATION);
  tft.setTextWrap(false);
  drawStatusScreen();
}

void setup() {
  Serial.begin(115200);
  initializeDisplay();
  connectToWiFi();
  configureWebServer();

  fetchServerStats();
  drawStatusScreen();
  lastPollMillis = millis();

  Serial.print("ESP32 dashboard: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  webServer.handleClient();

  if (millis() - lastPollMillis >= POLL_INTERVAL_MS) {
    lastPollMillis = millis();
    fetchServerStats();
    drawStatusScreen();
  }

  delay(2);
}
