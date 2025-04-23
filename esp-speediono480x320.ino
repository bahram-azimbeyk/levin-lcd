
#include "Comms.h"
#include "SPI.h"
#include <TFT_eSPI.h>
#include "levin.c"
TFT_eSPI tft = TFT_eSPI();

#define RXD2 16
#define TXD2 17

struct RecDetail {
  int x;
  int y;
  int color;
  int bg_color;
};
const int recCount = 30;
const int xValues[recCount] = { 140, 147, 153, 160, 166, 173, 179, 185, 191, 197, 204, 210, 216, 222, 229, 235, 242, 249, 255, 262, 269, 276, 284, 291, 298, 305, 313, 320, 327, 334 };
const int yValues[recCount] = { 174, 171, 168, 164, 161, 157, 154, 150, 147, 143, 140, 136, 133, 129, 125, 122, 120, 117, 115, 114, 113, 112, 112, 112, 112, 112, 112, 112, 112, 112 };

float volt = -1;
int rpm = -1;
int emap = -1;
int clt = -40;
int iat = -40;
int idle = -1;
int tps = -1;
float boost = -10;
int advance = -40;


bool connecting = false;

RecDetail recList[recCount];


void setup() {
  // set rev line rectangles position and color
  for (int i = 0; i < recCount; i++) {
    if (i < 27) {
      recList[i] = { xValues[i], yValues[i] - 5, TFT_GREEN, TFT_DARKGREEN };
    } else {
      recList[i] = { xValues[i], yValues[i] - 5, TFT_RED, TFT_MAROON };
    }
  }


  tft.init();
  tft.setRotation(1);         // Set the rotation as needed
  tft.fillScreen(TFT_BLACK);  // Clear the screen
  tft.setSwapBytes(true);
  tft.pushImage(40, 80, 400, 160, levin_logo);
  delay(2000);
  // Serial should be started after the tft
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.begin(9600);
}

void drawConnecting() {
  tft.fillScreen(TFT_BLACK);  // Clear the screen
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Connecting ...", 240, 160, 4);
}

void initTft() {
  resetPrevData();  // Reset previous data

  tft.fillScreen(TFT_BLACK);  // Clear the screen

  tft.pushImage(137, 130, 206, 82, rpm_line);  // draw the rpm line gauge

  // Draw the base rev rectangles
  for (int i = 0; i < recCount; i++) {
    tft.fillRect(recList[i].x, recList[i].y, 4, 20, recList[i].bg_color);
  }

  // Draw the base gauges shapes
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Engine RPM", 300, 150, 2);
  drawBaseGauge(76, 76, 66, "Battery", "Volt", 0, 18);
  drawBaseGauge(404, 76, 66, "CLT", "C", 0, 120);
  drawBaseGauge(76, 244, 66, "Engine Map", "kPa", 0, 200);
  drawBaseGauge(404, 244, 66, "IAT", "C", 0, 120);
  drawBaseBox(156, 14, "IAC Load", "");
  drawBaseBox(244, 14, "TPS", "%");
  drawBaseBox(156, 226, "Boost", "PSI");
  drawBaseBox(244, 226, "Advance", "deg");
}

void drawBaseBox(int x, int y, String label, String unit) {
  tft.drawRect(x, y, 80, 80, TFT_WHITE);
  tft.drawCentreString(label, x + 40, y + 10, 2);
  tft.drawCentreString(unit, x + 40, y + 55, 2);
}

void drawBaseGauge(int x, int y, int radius, String label, String unit, int min, int max) {
  tft.drawCircle(x, y, radius + 4, TFT_WHITE);
  tft.drawCircle(x, y, radius - 19, TFT_WHITE);

  tft.drawCentreString(label, x, y - (radius * 0.4), 2);
  tft.drawCentreString(unit, x, y + (radius * 0.4), 2);
  tft.drawCentreString(String(min), x - (radius * 0.45), y + (radius * 0.66), 2);
  tft.drawCentreString(String(max), x + (radius * 0.45), y + (radius * 0.66), 2);
}

void drawArcGauge(int x, int y, int radius, float value, float lastValue, int min, int max, int low, int high) {
  float startAngle = map(lastValue * 10, min * 10, max * 10, 45, 315);
  float endAngle = map(value * 10, min * 10, max * 10, 45, 315);  // Adjust the range as needed
  startAngle = constrain(startAngle, 45, 315);
  endAngle = constrain(endAngle, 45, 315);
  // Draw the arc
  if (lastValue >= low && value < low) {
    tft.drawArc(x, y, radius, radius - 15, 45, endAngle, TFT_BLUE, TFT_BLACK, true);
  } else if (lastValue <= high && value > high) {
    tft.drawArc(x, y, radius, radius - 15, 45, endAngle, TFT_RED, TFT_BLACK, true);
  } else if ((lastValue <= low && value >= low) || (lastValue >= high && value <= high)) {
    tft.drawArc(x, y, radius, radius - 15, 45, endAngle, TFT_GREEN, TFT_BLACK, true);
  } else if (startAngle < endAngle) {
    int color = TFT_GREEN;
    if (value < low) {
      color = TFT_BLUE;
    } else if (value > high) {
      color = TFT_RED;
    }
    tft.drawArc(x, y, radius, radius - 15, startAngle, endAngle, color, TFT_BLACK, true);
  };
  if (startAngle > endAngle) {
    tft.drawArc(x, y, radius, radius - 15, endAngle, startAngle, TFT_BLACK, TFT_BLACK, true);
  };
}

void loop() {
  // serial operation, frequency based request
  static uint32_t lastUpdate = millis();
  if (millis() - lastUpdate > 10) {
    requestData(20);
    lastUpdate = millis();
  }

  // get the data from the speediuno
  const float v = getByte(9) / 10.0;
  const int c = getByte(7) - 40;
  const int r = getWord(14);
  const int i = getByte(6) - 40;
  const int m = getWord(4);
  const int s = getWord(37);
  const int t = getByte(24) / 2;
  const int ba = getByte(40);
  const float b = (m - ba) / 6.895;
  const int a = getByte(23);

  // check if the data is invalid show connecting
  if (c == -40 || i == -40) {
    if (connecting == false) {
      drawConnecting();
      connecting = true;
    };
    delay(500);
  } else {
    if (connecting == true) {
      initTft();
      connecting = false;
    };
    drawVolt(v);
    drawClt(c);
    drawIat(i);
    drawMap(m, ba);
    drawRpm(r);

    drawIdle(s);
    drawTps(t);
    drawBoost(b);
    drawAdvance(a);
    delay(20);
  };
}

void drawIdle(int value) {
  if (value != idle) {
    const int x = 156;
    const int y = 14;
    value = constrain(value, 0, 300);
    if ((idle > 9.95 && value < 9.95) || (idle > 99.95 && value < 99.95)) {
      tft.fillRect(x + 10, y + 30, 60, 25, TFT_BLACK);
    };
    tft.drawCentreString(String(value), x + 40, y + 30, 4);
    idle = value;
  };
}

void drawTps(int value) {
  if (value != tps) {
    const int x = 244;
    const int y = 14;
    value = constrain(value, 0, 100);
    if ((tps > 9.95 && value < 9.95) || (tps > 99.95 && value < 99.95)) {
      tft.fillRect(x + 10, y + 30, 60, 25, TFT_BLACK);
    };
    tft.drawCentreString(String(value), x + 40, y + 30, 4);
    tps = value;
  };
}

void drawBoost(float value) {
  if (value != boost) {
    const int x = 156;
    const int y = 226;
    value = constrain(value, -9.9, 40);
    if ((boost > 9.95 && value < 9.95) || (boost < -0.05 && value > -0.05)) {
      tft.fillRect(x + 10, y + 30, 60, 25, TFT_BLACK);
    };
    tft.drawCentreString(String(value, 1), x + 40, y + 30, 4);
    boost = value;
  };
}

void drawAdvance(int value) {
  if (value != advance) {
    const int x = 244;
    const int y = 226;
    value = constrain(value, -60, 80);
    if ((advance > 9.95 && value < 9.95) || (advance < -0.05 && value > -0.05) || (advance < -10.05 && value > -10.05)) {
      tft.fillRect(x + 10, y + 30, 60, 25, TFT_BLACK);
    };
    tft.drawCentreString(String(value), x + 40, y + 30, 4);
    advance = value;
  };
}

void drawVolt(float value) {
  const int x = 76;
  const int y = 76;
  if (value != volt) {
    value = constrain(value, 0, 24);
    if (volt > 9.95 && value < 9.95) {
      tft.fillRect(x - 30, y - 10, 60, 25, TFT_BLACK);
    };
    drawArcGauge(x, y, 66, value, volt, 0, 18, 11, 15);
    tft.drawCentreString(String(value, 1), x, y - 10, 4);
    volt = value;
  };
}

void drawClt(int value) {
  const int x = 404;
  const int y = 76;
  if (value != clt) {
    value = constrain(value, -40, 150);
    if ((clt > 99.95 && value < 99.95) || (clt > 9.95 && value < 9.95) || (clt < -0.05 && value > -0.05) || (clt < -10.05 && value > -10.05)) {
      tft.fillRect(x - 30, y - 10, 60, 25, TFT_BLACK);
    };
    drawArcGauge(x, y, 66, value, clt, 0, 120, 60, 100);
    tft.drawCentreString(String(value), x, y - 10, 4);
    clt = value;
  };
}

void drawMap(int value, int baroValue) {
  const int x = 76;
  const int y = 244;
  if (value != emap) {
    value = constrain(value, 0, 300);
    if (emap > 99.5 && value < 99.5) {
      tft.fillRect(x - 30, y - 10, 60, 25, TFT_BLACK);
    };
    drawArcGauge(x, y, 66, value, emap, 0, 200, baroValue, 150);
    tft.drawCentreString(String(value), x, y - 10, 4);
    emap = value;
  };
}

void drawIat(int value) {
  const int x = 404;
  const int y = 244;
  if (value != iat) {
    value = constrain(value, -40, 150);
    if ((iat > 99.95 && value < 99.95) || (iat > 9.95 && value < 9.95) || (iat < -0.05 && value > -0.05) || (iat < -10.05 && value > -10.05)) {
      tft.fillRect(x - 30, y - 10, 60, 25, TFT_BLACK);
    };
    drawArcGauge(x, y, 66, value, iat, 0, 120, 35, 85);
    tft.drawCentreString(String(value), x, y - 10, 4);
    iat = value;
  };
}

void drawRpm(int value) {
  if (value != rpm) {
    value = constrain(value, 0, 9999);
    if ((rpm > 999.95 && value < 999.95) || (rpm > 99.95 && value < 99.95) || (rpm > 9.95 && value < 9.95)) {
      tft.fillRect(300 - 30, 170, 60, 25, TFT_BLACK);
    };
    tft.drawCentreString(String(value), 300, 170, 4);

    int start = map(rpm, 0, 7000, 0, 29);
    int end = map(value, 0, 7000, 0, 29);
    start = constrain(start, 0, 29);
    end = constrain(end, 0, 29);

    // Draw Rpm
    if (start < end) {
      for (int i = start; i < end; i++) {
        tft.fillRect(recList[i].x, recList[i].y, 4, 20, recList[i].color);
      }
    } else if (start > end) {
      for (int i = end; i < start; i++) {
        tft.fillRect(recList[i].x, recList[i].y, 4, 20, recList[i].bg_color);
      }
    };

    rpm = value;
  };
}

void resetPrevData() {
  volt = -1;
  rpm = -1;
  emap = -1;
  clt = -40;
  iat = -40;
  idle = -1;
  tps = -1;
  boost = -10;
  advance = -40;
}
