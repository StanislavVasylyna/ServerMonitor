#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4
#define TFT_SCLK  18
#define TFT_MOSI  23
#define TFT_BL    15

#define ENC_A     25   // S1
#define ENC_B     26   // S2
#define ENC_BTN   27   // KEY

Adafruit_ST7735 tft = Adafruit_ST7735(
  TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST
);

bool whiteBg = false;
int brightness = 160;

int lastA;
bool lastBtn = HIGH;

void setup() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);

  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, brightness);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);

  lastA = digitalRead(ENC_A);

  drawScreen();
}

void loop() {
  handleEncoder();
  handleButton();
}

void handleEncoder() {
  int currentA = digitalRead(ENC_A);

  if (currentA != lastA) {
    if (digitalRead(ENC_B) != currentA) {
      brightness += 8;
    } else {
      brightness -= 8;
    }

    brightness = constrain(brightness, 0, 255);
    ledcWrite(TFT_BL, brightness);

    lastA = currentA;
    delay(2);
  }
}

void handleButton() {
  bool btn = digitalRead(ENC_BTN);

  if (lastBtn == HIGH && btn == LOW) {
    whiteBg = !whiteBg;
    drawScreen();
    delay(200);
  }

  lastBtn = btn;
}

void drawScreen() {
  uint16_t bg = whiteBg ? ST77XX_WHITE : ST77XX_BLACK;
  uint16_t frame = whiteBg ? ST77XX_BLACK : ST77XX_WHITE;

  tft.fillScreen(bg);

  drawBarFrame(10, 30, 140, 18, frame);
  drawBarFrame(10, 60, 140, 18, frame);
  drawBarFrame(10, 90, 140, 18, frame);

  fillBar(10, 30, 140, 18, 35, ST77XX_GREEN, bg);
  fillBar(10, 60, 140, 18, 65, ST77XX_BLUE, bg);
  fillBar(10, 90, 140, 18, 78, ST77XX_RED, bg);
}

void drawBarFrame(int x, int y, int w, int h, uint16_t color) {
  tft.drawRect(x, y, w, h, color);
}

void fillBar(int x, int y, int w, int h, int percent, uint16_t color, uint16_t bg) {
  int innerW = w - 2;
  int fillW = map(percent, 0, 100, 0, innerW);

  tft.fillRect(x + 1, y + 1, innerW, h - 2, bg);
  tft.fillRect(x + 1, y + 1, fillW, h - 2, color);
}
