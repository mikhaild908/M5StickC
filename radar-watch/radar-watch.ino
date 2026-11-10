#include <M5StickC.h>
#include <math.h>

namespace {

const uint16_t COLOR_BACKGROUND = 0x0000;
const uint16_t COLOR_DIM = 0x0186;
const uint16_t COLOR_GRID = 0x0328;
const uint16_t COLOR_TEAL = 0x04D0;
const uint16_t COLOR_CYAN = 0x5DFF;
const uint16_t COLOR_BRIGHT = 0xDFFF;
const uint16_t COLOR_AMBER = 0xFD20;

const int16_t RADAR_X = 120;
const int16_t RADAR_Y = 45;
const int16_t RADAR_RADIUS = 29;
const uint32_t RADAR_FRAME_MS = 45;
const uint32_t PULSE_DURATION_MS = 1200;

const uint8_t DIGITS[10][7] = {
    {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},
    {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},
    {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},
    {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b10001, 0b01110},
    {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},
    {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b10001, 0b01110},
    {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},
    {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},
    {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b11100},
};

uint32_t lastClockReadAt = 0;
uint32_t lastRadarFrameAt = 0;
uint32_t pulseStartedAt = 0;
uint8_t lastDisplayedHour = 255;
uint8_t lastDisplayedMinute = 255;
uint8_t lastDisplayedAmPm = 255;
uint16_t lastSweepAngle = 0;
uint8_t lastPulseRadius = 0;
bool sweepDrawn = false;
bool pulseTriggered = false;

uint8_t getArizonaHour() {
  const int16_t utcMinutes =
      static_cast<int16_t>(M5.Rtc.Hour) * 60 + M5.Rtc.Minute;
  const int16_t arizonaMinutes = (utcMinutes + 1440 - 420) % 1440;
  return static_cast<uint8_t>(arizonaMinutes / 60);
}

void drawDigit(uint8_t digit, int16_t x, int16_t y) {
  for (uint8_t row = 0; row < 7; ++row) {
    for (uint8_t column = 0; column < 5; ++column) {
      if (DIGITS[digit][row] & (1 << (4 - column))) {
        M5.Lcd.fillRect(x + column * 2, y + row * 2, 2, 2, COLOR_CYAN);
      }
    }
  }
}

void drawClock() {
  const uint8_t hour24 = getArizonaHour();
  const uint8_t hour12 = (hour24 % 12 == 0) ? 12 : hour24 % 12;
  const uint8_t minute = M5.Rtc.Minute;
  const uint8_t amPm = hour24 < 12 ? 0 : 1;

  M5.Lcd.fillRect(5, 20, 80, 31, COLOR_BACKGROUND);
  drawDigit(hour12 / 10, 7, 27);
  drawDigit(hour12 % 10, 21, 27);
  M5.Lcd.fillRect(36, 31, 3, 3, COLOR_AMBER);
  M5.Lcd.fillRect(36, 39, 3, 3, COLOR_AMBER);
  drawDigit(minute / 10, 45, 27);
  drawDigit(minute % 10, 59, 27);

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(COLOR_AMBER, COLOR_BACKGROUND);
  M5.Lcd.setCursor(72, 31);
  M5.Lcd.print(amPm == 0 ? "AM" : "PM");

  lastDisplayedHour = hour12;
  lastDisplayedMinute = minute;
  lastDisplayedAmPm = amPm;
}

void drawRadarGrid() {
  M5.Lcd.drawCircle(RADAR_X, RADAR_Y, RADAR_RADIUS, COLOR_TEAL);
  M5.Lcd.drawCircle(RADAR_X, RADAR_Y, 20, COLOR_GRID);
  M5.Lcd.drawCircle(RADAR_X, RADAR_Y, 10, COLOR_GRID);
  M5.Lcd.drawFastHLine(RADAR_X - RADAR_RADIUS, RADAR_Y,
                       RADAR_RADIUS * 2 + 1, COLOR_GRID);
  M5.Lcd.drawFastVLine(RADAR_X, RADAR_Y - RADAR_RADIUS,
                       RADAR_RADIUS * 2 + 1, COLOR_GRID);

  M5.Lcd.fillCircle(130, 32, 2, COLOR_AMBER);
  M5.Lcd.fillCircle(107, 52, 2, COLOR_TEAL);
  M5.Lcd.fillCircle(136, 57, 1, COLOR_TEAL);
  M5.Lcd.drawPixel(RADAR_X, RADAR_Y, COLOR_BRIGHT);
}

void drawRay(uint16_t angle, uint16_t color) {
  const float radians = angle * 0.0174532925f;
  const int16_t x = RADAR_X + static_cast<int16_t>(cosf(radians) * RADAR_RADIUS);
  const int16_t y = RADAR_Y + static_cast<int16_t>(sinf(radians) * RADAR_RADIUS);
  M5.Lcd.drawLine(RADAR_X, RADAR_Y, x, y, color);
}

void drawSweep(uint16_t angle, bool erase) {
  drawRay((angle + 352) % 360, erase ? COLOR_BACKGROUND : COLOR_GRID);
  drawRay((angle + 356) % 360, erase ? COLOR_BACKGROUND : COLOR_TEAL);
  drawRay(angle, erase ? COLOR_BACKGROUND : COLOR_BRIGHT);
}

void drawStaticFace() {
  M5.Lcd.fillScreen(COLOR_BACKGROUND);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextWrap(false);
  M5.Lcd.setTextColor(COLOR_TEAL, COLOR_BACKGROUND);
  M5.Lcd.setCursor(7, 5);
  M5.Lcd.print("PHOENIX  /  MST");

  M5.Lcd.drawFastHLine(5, 15, 150, COLOR_GRID);
  M5.Lcd.drawFastVLine(87, 19, 55, COLOR_GRID);
  M5.Lcd.setTextColor(COLOR_DIM, COLOR_BACKGROUND);
  M5.Lcd.setCursor(8, 51);
  M5.Lcd.print("ARIZONA");
  M5.Lcd.setCursor(8, 60);
  M5.Lcd.print("NO DST  /  12H");

  M5.Lcd.drawFastHLine(5, 78, 76, COLOR_GRID);
  M5.Lcd.drawFastHLine(91, 78, 58, COLOR_GRID);
  M5.Lcd.setTextColor(COLOR_AMBER, COLOR_BACKGROUND);
  M5.Lcd.setCursor(7, 69);
  M5.Lcd.print("A: PULSE");

  drawRadarGrid();
}

void updateRadar(uint32_t now) {
  if (now - lastRadarFrameAt < RADAR_FRAME_MS) {
    return;
  }
  lastRadarFrameAt = now;

  if (sweepDrawn) {
    drawSweep(lastSweepAngle, true);
  }
  if (lastPulseRadius > 0) {
    M5.Lcd.drawCircle(RADAR_X, RADAR_Y, lastPulseRadius, COLOR_BACKGROUND);
  }
  drawRadarGrid();

  uint8_t pulseRadius = 0;
  if (pulseTriggered) {
    const uint32_t elapsed = now - pulseStartedAt;
    if (elapsed < PULSE_DURATION_MS) {
      pulseRadius = 4 + (elapsed * (RADAR_RADIUS - 4)) / PULSE_DURATION_MS;
      M5.Lcd.drawCircle(RADAR_X, RADAR_Y, pulseRadius,
                        (elapsed / 120) % 2 == 0 ? COLOR_BRIGHT : COLOR_CYAN);
    } else {
      pulseTriggered = false;
    }
  }

  const uint16_t angle = static_cast<uint16_t>((now / 24) % 360);
  drawSweep(angle, false);
  lastSweepAngle = angle;
  lastPulseRadius = pulseRadius;
  sweepDrawn = true;
}

}  // namespace

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Rtc.GetBm8563Time();

  drawStaticFace();
  drawClock();
  lastClockReadAt = millis();
  lastRadarFrameAt = lastClockReadAt - RADAR_FRAME_MS;
}

void loop() {
  M5.update();
  const uint32_t now = millis();

  if (M5.BtnA.wasPressed()) {
    pulseStartedAt = now;
    pulseTriggered = true;
  }

  if (now - lastClockReadAt >= 1000) {
    M5.Rtc.GetBm8563Time();
    lastClockReadAt = now;

    const uint8_t hour24 = getArizonaHour();
    const uint8_t hour12 = (hour24 % 12 == 0) ? 12 : hour24 % 12;
    const uint8_t amPm = hour24 < 12 ? 0 : 1;
    if (hour12 != lastDisplayedHour || M5.Rtc.Minute != lastDisplayedMinute ||
        amPm != lastDisplayedAmPm) {
      drawClock();
    }
  }

  updateRadar(now);
}
