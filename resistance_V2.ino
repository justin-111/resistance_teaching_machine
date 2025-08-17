#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>

// OLED 初始化
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// NeoPixel LED 設定
#define LED_PIN 12
#define LED_COUNT 8
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// 腳位與參數設定
#define POWER_PIN 2
#define MEASURE_PIN 3
#define SENSE_PIN A3       // 量測分壓點
const float R_ref = 22000.0; // 參考電阻 1kΩ
const float Vcc = 5.0;      // 供應電壓

float lastRx = 0.0;
uint32_t lastColors[4];
bool firstRun = true;

void setup() {
  pinMode(POWER_PIN, OUTPUT);
  pinMode(MEASURE_PIN, OUTPUT);
  Serial.begin(9600);

  u8g2.begin();
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.setContrast(128);
  displaySplashScreen();

  strip.begin();
  strip.show();
  strip.setBrightness(80);
}

void displaySplashScreen() {
  u8g2.clearBuffer();
  u8g2.drawStr(25, 40, "v2.0 OLED");
  u8g2.drawStr(20, 55, "Initializing...");
  u8g2.sendBuffer();
  delay(1500);
}

// 平均多次ADC讀值，回傳電壓
float readVoltage(int pin) {
  digitalWrite(POWER_PIN, HIGH);
  delayMicroseconds(200);
  float sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(pin);
    delayMicroseconds(50);
  }
  digitalWrite(POWER_PIN, LOW);
  return (sum / 50.0) * (Vcc / 1023.0);
}

void displayMeasurement(float rxValue) {
  char buffer[20];
  char unit[6];
  if (rxValue >= 1e6) {
    rxValue /= 1e6;
    strcpy(unit, "Mohm");
  } else if (rxValue >= 1e3) {
    rxValue /= 1e3;
    strcpy(unit, "kohm");
  } else {
    strcpy(unit, "ohm");
  }
  dtostrf(rxValue, 7, 2, buffer);
  u8g2.clearBuffer();
  u8g2.drawStr(5, 15, "Resistance:");
  u8g2.setFont(u8g2_font_logisoso24_tf);
  u8g2.setCursor(10, 50);
  u8g2.print(buffer);
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.setCursor(95, 60);
  u8g2.print(unit);
  u8g2.sendBuffer();
}

uint32_t resistorColorCode(int code) {
  switch (code) {
    case 0: return strip.Color(0, 0, 0);
    case 1: return strip.Color(139, 69, 19);
    case 2: return strip.Color(255, 0, 0);
    case 3: return strip.Color(255, 165, 0);
    case 4: return strip.Color(255, 255, 0);
    case 5: return strip.Color(0, 255, 0);
    case 6: return strip.Color(0, 0, 255);
    case 7: return strip.Color(128, 0, 128);
    case 8: return strip.Color(128, 128, 128);
    case 9: return strip.Color(255, 255, 255);
    case 10: return strip.Color(218, 165, 32);
    case 11: return strip.Color(192, 192, 192);
    default: return strip.Color(0, 0, 0);
  }
}

bool shouldUpdateLED(float currentRx) {
  if (firstRun) {
    firstRun = false;
    lastRx = currentRx;
    return true;
  }
  if (currentRx < 1.0 || currentRx >= 1e9) {
    if (lastRx >= 1.0 && lastRx < 1e9) {
      lastRx = currentRx;
      return true;
    }
    return false;
  }
  float changePercent = fabs((currentRx - lastRx) / lastRx) * 100.0;
  if (changePercent > 5.0) {
    lastRx = currentRx;
    return true;
  }
  return false;
}

void displayResistorColorBands(float rx) {
  int digit1 = 0, digit2 = 0, multiplier = 0;
  if (rx < 1.0 || rx >= 1e9) {
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
    for (int group = 0; group < 4; group++) {
      lastColors[group] = strip.Color(0, 0, 0);
    }
    return;
  }
  long rxInt = (long)(rx + 0.5);
  while (rxInt >= 100) {
    rxInt /= 10;
    multiplier++;
  }
  digit1 = rxInt / 10;
  digit2 = rxInt % 10;
  if (multiplier > 9) multiplier = 9;
  uint32_t colors[4] = {
    resistorColorCode(digit1),
    resistorColorCode(digit2),
    resistorColorCode(multiplier),
    resistorColorCode(10)
  };
  for (int group = 0; group < 4; group++) {
    lastColors[group] = colors[group];
  }
  for (int group = 0; group < 4; group++) {
    for (int i = 0; i < 2; i++) {
      strip.setPixelColor(group * 2 + i, colors[group]);
    }
  }
  strip.show();
}

void loop() {
  digitalWrite(MEASURE_PIN, HIGH);
  delay(10);
  float Vout = readVoltage(SENSE_PIN);
  digitalWrite(MEASURE_PIN, LOW);

  float Rx = 0;
  if (Vout > 0.01 && Vout < (Vcc - 0.01)) {
    Rx = R_ref * (Vcc / Vout - 1); // 經典分壓公式
  } else {
    Rx = 0;
  }

  displayMeasurement(Rx);
  if (shouldUpdateLED(Rx)) {
    displayResistorColorBands(Rx);
  }

  Serial.print("Vout: ");
  Serial.println(Vout);
  Serial.print("Rx: ");
  Serial.println(Rx);
  delay(2000);
}
