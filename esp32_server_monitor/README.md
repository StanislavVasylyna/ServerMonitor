# ESP32 Server Monitor

ESP32 опитує `server_monitor_server` через `GET /stats`, показує CPU, RAM і
диск на SPI TFT ST7735 та запускає локальну веб-сторінку з такими самими
прогрес-барами.

## Потрібні компоненти

- ESP32 DevKit;
- TFT ST7735 1.44" 128x128 або 1.8" 128x160, SPI;
- бібліотеки Arduino:
  - `Adafruit GFX Library`;
  - `Adafruit ST7735 and ST7789 Library`;
  - `ArduinoJson` версії 6.x.

`WiFi`, `HTTPClient` і `WebServer` входять до ESP32 Arduino Core.

## Підключення

| TFT | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SCL / SCK | GPIO 18 |
| SDA / MOSI | GPIO 23 |
| CS | GPIO 5 |
| DC / A0 | GPIO 2 |
| RST / RES | GPIO 4 |
| LED / BL | 3.3V |

Не подавайте 5V на сигнальні входи дисплея. Якщо модуль має інші позначення
або вбудований стабілізатор, звіртеся з його схемою.

## Налаштування

У `config.h` задайте:

```cpp
#define WIFI_SSID "MyWiFi"
#define WIFI_PASSWORD "secret"
#define SERVER_STATS_URL "http://192.168.1.50:8000/stats"
```

`localhost` використовувати не можна: для ESP32 це адреса самого ESP32.
Вкажіть LAN IP комп'ютера, де запущено `server_monitor_server/main.py`.

Для дисплея 1.44" 128x128:

```cpp
#define TFT_PANEL_HEIGHT 128
```

Для дисплея 1.8" 128x160:

```cpp
#define TFT_PANEL_HEIGHT 160
```

Якщо зображення повернуте, змініть `TFT_ROTATION` на `1`, `2` або `3`.
Якщо кольори чи зміщення неправильні, конкретна ревізія ST7735 може вимагати
іншого значення `INITR_*` у `initializeDisplay()`.

## Запуск

1. Запустіть API:

   ```bash
   cd server_monitor_server
   python3 main.py
   ```

2. Відкрийте `esp32_server_monitor.ino` в Arduino IDE.
3. Встановіть зазначені бібліотеки та виберіть свою ESP32 board.
4. Прошийте ESP32 і відкрийте Serial Monitor на `115200`.
5. У Serial Monitor буде адреса веб-сторінки ESP32, наприклад
   `http://192.168.1.80/`.
