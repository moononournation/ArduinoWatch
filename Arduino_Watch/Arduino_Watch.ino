/*
   Arduino Watch Pro Micro Version
   - read current time from RTC
   - draw analog clock face every loop
   - shutdown power after 60 seconds
   - press power button to restart
*/
// real time clock class (RTC_DS1307 / RTC_DS3231 / RTC_PCF8523)
// comment out if no RTC at all
#define RTC_CLASS RTC_DS3231

// auto sleep time (in milliseconds)
// comment out if no need enter sleep, e.g. still in developing stage
#define SLEEP_TIME 20000L
#define BACKGROUND BLACK
#define MARK_COLOR WHITE
#define SUBMARK_COLOR 0x7BEF //0xBDF7
#define HOUR_COLOR WHITE
#define MINUTE_COLOR BLUE // 0x7BEF
#define SECOND_COLOR RED

#define HOUR_LEN 50
#define MINUTE_LEN 90
#define SECOND_LEN 100

#define SIXTIETH 0.016666667
#define TWELFTH 0.08333333
#define SIXTIETH_RADIAN 0.10471976
#define TWELFTH_RADIAN 0.52359878
#define RIGHT_ANGLE_RADIAN 1.5707963
#define CENTER 120

#if defined (ESP32)
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 17
#define TFT_BL 22
#else
#define TFT_DC 19
#define TFT_RST 18
#define TFT_BL 10
#endif
#define TFT_ROTATION 2
#define TFT_IPS true
#define LED_LEVEL 128

#define WAKEUP_PIN 7

#include "Debug_Printer.h"
#include <SPI.h>
#include <Arduino_HWSPI.h>
#include <Arduino_GFX.h>
#include <Arduino_TFT.h>
#include <Arduino_ST7789.h> // Hardware-specific library for ST7789 (with or without CS pin)

#ifdef SLEEP_TIME
#if defined (ESP32)
#include <esp_sleep.h>
#elif defined (__AVR__)
#include <LowPower.h>
#endif
#endif // #ifdef SLEEP_TIME

#ifdef RTC_CLASS
#include <Wire.h>
#include "RTClib.h"
RTC_CLASS rtc;
#else
static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  return (10 * (*p - '0')) + (*++p - '0');
}
#endif

//You can use different type of hardware initialization
#ifdef TFT_CS
Arduino_HWSPI *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
#else
Arduino_HWSPI *bus = new Arduino_HWSPI(TFT_DC); //for display without CS pin
#endif
Arduino_ST7789 *tft = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, 240, 240, 0, 80, TFT_IPS); // 1.3"/1.5" square IPS LCD

float sdeg, mdeg, hdeg;
uint8_t osx = CENTER, osy = CENTER, omx = CENTER, omy = CENTER, ohx = CENTER, ohy = CENTER; // Saved H, M, S x & y coords
uint8_t nsx, nsy, nmx, nmy, nhx, nhy;                                                       // H, M, S x & y coords
uint8_t xMin, yMin, xMax, yMax;                                                             // redraw range
uint8_t hh, mm, ss;
unsigned long targetTime, sleepTime; // next action time
#ifdef TFT_BL
bool ledTurnedOn = false;
#endif
#ifdef DEBUG_MODE
static uint16_t loopCount = 0;
#endif

static uint8_t cached_points[HOUR_LEN + 1 + MINUTE_LEN + 1 + SECOND_LEN + 1][2];
static int cached_points_idx = 0;

void setup(void)
{
  DEBUG_BEGIN(115200);
  DEBUG_PRINTMLN(": start");

#if defined (ESP32)
  tft->begin(80000000);
#else
  tft->begin();
#endif
  DEBUG_PRINTMLN(": tft->begin()");

  tft->fillScreen(BACKGROUND);
  DEBUG_PRINTMLN(": tft->fillScreen(BACKGROUND)");

  pinMode(WAKEUP_PIN, INPUT_PULLUP);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
#endif

  //draw_round_clock_mark(104, 120, 112, 120, 114, 114);
  draw_square_clock_mark(102, 120, 108, 120, 114, 120);
  //draw_square_clock_mark(72, 90, 78, 90, 84, 90);
  DEBUG_PRINTMLN(": Draw 60 clock marks");

#ifdef RTC_CLASS
  // read date and time from RTC
  read_rtc();
#else
  hh = conv2d(__TIME__);
  mm = conv2d(__TIME__ + 3);
  ss = conv2d(__TIME__ + 6);
#endif

  targetTime = ((millis() / 1000) + 1) * 1000;

#ifdef SLEEP_TIME
  sleepTime = millis() + SLEEP_TIME;
#endif
}

void loop()
{
  unsigned long cur_millis = millis();
  if (cur_millis >= targetTime)
  {
    targetTime += 1000;
    ss++; // Advance second
    if (ss == 60)
    {
      ss = 0;
      mm++; // Advance minute
      if (mm > 59)
      {
        mm = 0;
        hh++; // Advance hour
        if (hh > 23)
        {
          hh = 0;
        }
      }
    }
  }

  // Pre-compute hand degrees, x & y coords for a fast screen update
  sdeg = SIXTIETH_RADIAN * ((0.001 * (cur_millis % 1000)) + ss); // 0-59 (includes millis)
  nsx = cos(sdeg - RIGHT_ANGLE_RADIAN) * SECOND_LEN + CENTER;
  nsy = sin(sdeg - RIGHT_ANGLE_RADIAN) * SECOND_LEN + CENTER;
  if ((nsx != osx) || (nsy != osy))
  {
    mdeg = (SIXTIETH * sdeg) + (SIXTIETH_RADIAN * mm); // 0-59 (includes seconds)
    hdeg = (TWELFTH * mdeg) + (TWELFTH_RADIAN * hh);   // 0-11 (includes minutes)
    mdeg -= RIGHT_ANGLE_RADIAN;
    hdeg -= RIGHT_ANGLE_RADIAN;
    nmx = cos(mdeg) * MINUTE_LEN + CENTER;
    nmy = sin(mdeg) * MINUTE_LEN + CENTER;
    nhx = cos(hdeg) * HOUR_LEN + CENTER;
    nhy = sin(hdeg) * HOUR_LEN + CENTER;

    // redraw hands
    redraw_hands_cached_lines();
    // redraw_hands_4_split(spi_raw_overwrite_rect);

    ohx = nhx;
    ohy = nhy;
    omx = nmx;
    omy = nmy;
    osx = nsx;
    osy = nsy;

#ifdef TFT_BL
    if (!ledTurnedOn)
    {
      backlight(true);
    }
#endif

#ifdef DEBUG_MODE
    loopCount++;
#endif
  }

#ifdef SLEEP_TIME
  if (cur_millis > sleepTime)
  {
    // enter sleep
#ifdef DEBUG_MODE
    DEBUG_PRINTM(": enter sleep, loopCount: ");
    DEBUG_PRINTLN(loopCount);
    delay(10);
#endif

    enterSleep();
    loopCount = 0;
  }
#endif

  delay(1);
}

#ifdef TFT_BL
void backlight(bool enable) {
  if (enable) {
#if defined (ESP32)
    ledcAttachPin(TFT_BL, 1); // assign TFT_BL pin to channel 1
    ledcSetup(1, 12000, 8); // 12 kHz PWM, 8-bit resolution
    ledcWrite(1, LED_LEVEL); // brightness 0 - 255
#else
    analogWrite(TFT_BL, LED_LEVEL);
#endif
    ledTurnedOn = true;
  } else {
    digitalWrite(TFT_BL, LOW);
    ledTurnedOn = false;
  }
}
#endif

#ifdef RTC_CLASS
void read_rtc()
{
  Wire.begin();
  rtc.begin();
  DateTime now = rtc.now();
  hh = now.hour();
  mm = now.minute();
  ss = now.second();
  DEBUG_PRINTM(": read date and time from RTC: ");
  DEBUG_PRINT(hh);
  DEBUG_PRINT(":");
  DEBUG_PRINT(mm);
  DEBUG_PRINT(":");
  DEBUG_PRINTLN(ss);
  Wire.end();
}
#endif

#ifdef SLEEP_TIME
void enterSleep()
{
#ifdef TFT_BL
  backlight(false);
#endif
  tft->displayOff();

#if defined (ESP32)
  gpio_wakeup_enable(WAKEUP_PIN, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
#elif defined (__AVR__)
  // Allow wake up pin to trigger interrupt on low.
  attachInterrupt(digitalPinToInterrupt(WAKEUP_PIN), wakeUp, LOW);

  // Enter power down state with ADC and BOD module disabled.
  // Wake up when wake up pin is low.
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  // Disable external pin interrupt on wake up pin.
  detachInterrupt(digitalPinToInterrupt(WAKEUP_PIN));
#endif

  tft->displayOn();

  // read date and time from RTC
#ifdef RTC_CLASS
  read_rtc();
#endif

#ifdef SLEEP_TIME
  sleepTime = millis() + SLEEP_TIME;
#endif
}
#endif // #ifdef SLEEP_TIME

void wakeUp()
{
  // Just a handler for the pin interrupt.
}

void draw_round_clock_mark(uint8_t innerR1, uint8_t outerR1, uint8_t innerR2, uint8_t outerR2, uint8_t innerR3, uint8_t outerR3)
{
  float x, y;
  uint8_t x0, x1, y0, y1, innerR, outerR;
  uint16_t c;

  for (int i = 0; i < 60; i++)
  {
    if ((i % 15) == 0)
    {
      innerR = innerR1;
      outerR = outerR1;
      c = MARK_COLOR;
    }
    else if ((i % 5) == 0)
    {
      innerR = innerR2;
      outerR = outerR2;
      c = MARK_COLOR;
    }
    else
    {
      innerR = innerR3;
      outerR = outerR3;
      c = SUBMARK_COLOR;
    }

    mdeg = (SIXTIETH_RADIAN * i) - RIGHT_ANGLE_RADIAN;
    x = cos(mdeg);
    y = sin(mdeg);
    x0 = x * outerR + CENTER;
    y0 = y * outerR + CENTER;
    x1 = x * innerR + CENTER;
    y1 = y * innerR + CENTER;

    tft->drawLine(x0, y0, x1, y1, c);
    DEBUG_PRINTM(" i: ");
    DEBUG_PRINT(i);
    DEBUG_PRINT(", x0: ");
    DEBUG_PRINT(x0);
    DEBUG_PRINT(", y0: ");
    DEBUG_PRINT(y0);
    DEBUG_PRINT(", x1: ");
    DEBUG_PRINT(x1);
    DEBUG_PRINT(", y1: ");
    DEBUG_PRINTLN(y1);
  }
}

void draw_square_clock_mark(uint8_t innerR1, uint8_t outerR1, uint8_t innerR2, uint8_t outerR2, uint8_t innerR3, uint8_t outerR3)
{
  float x, y;
  uint8_t x0, x1, y0, y1, innerR, outerR;
  uint16_t c;

  for (int i = 0; i < 60; i++)
  {
    if ((i % 15) == 0)
    {
      innerR = innerR1;
      outerR = outerR1;
      c = MARK_COLOR;
    }
    else if ((i % 5) == 0)
    {
      innerR = innerR2;
      outerR = outerR2;
      c = MARK_COLOR;
    }
    else
    {
      innerR = innerR3;
      outerR = outerR3;
      c = SUBMARK_COLOR;
    }

    if ((i >= 53) || (i < 8))
    {
      x = tan(SIXTIETH_RADIAN * i);
      x0 = CENTER + (x * outerR);
      y0 = CENTER + (1 - outerR);
      x1 = CENTER + (x * innerR);
      y1 = CENTER + (1 - innerR);
    }
    else if (i < 23)
    {
      y = tan((SIXTIETH_RADIAN * i) - RIGHT_ANGLE_RADIAN);
      x0 = CENTER + (outerR);
      y0 = CENTER + (y * outerR);
      x1 = CENTER + (innerR);
      y1 = CENTER + (y * innerR);
    }
    else if (i < 38)
    {
      x = tan(SIXTIETH_RADIAN * i);
      x0 = CENTER - (x * outerR);
      y0 = CENTER + (outerR);
      x1 = CENTER - (x * innerR);
      y1 = CENTER + (innerR);
    }
    else if (i < 53)
    {
      y = tan((SIXTIETH_RADIAN * i) - RIGHT_ANGLE_RADIAN);
      x0 = CENTER + (1 - outerR);
      y0 = CENTER - (y * outerR);
      x1 = CENTER + (1 - innerR);
      y1 = CENTER - (y * innerR);
    }
    tft->drawLine(x0, y0, x1, y1, c);
    DEBUG_PRINTM(" i: ");
    DEBUG_PRINT(i);
    DEBUG_PRINT(", x0: ");
    DEBUG_PRINT(x0);
    DEBUG_PRINT(", y0: ");
    DEBUG_PRINT(y0);
    DEBUG_PRINT(", x1: ");
    DEBUG_PRINT(x1);
    DEBUG_PRINT(", y1: ");
    DEBUG_PRINTLN(y1);
  }
}

void redraw_hands_cached_lines()
{
  cached_points_idx = 0;
  write_cached_line(nhx, nhy, CENTER, CENTER, HOUR_COLOR, true, false);   // cache new hour hand
  write_cached_line(nsx, nsy, CENTER, CENTER, SECOND_COLOR, true, false); // cache new second hand
  tft->startWrite();
  write_cached_line(nmx, nmy, CENTER, CENTER, MINUTE_COLOR, true, true); // cache and draw new minute hand
  write_cached_line(omx, omy, CENTER, CENTER, BACKGROUND, false, true);  // earse old minute hand
  tft->writeLine(nhx, nhy, CENTER, CENTER, HOUR_COLOR);                  // draw new hour hand
  write_cached_line(ohx, ohy, CENTER, CENTER, BACKGROUND, false, true);  // earse old hour hand
  tft->writeLine(nsx, nsy, CENTER, CENTER, SECOND_COLOR);                // draw new second hand
  write_cached_line(osx, osy, CENTER, CENTER, BACKGROUND, false, true);  // earse old second hand
  tft->endWrite();
}

void write_cached_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color, bool save, bool actual_draw)
{
#if defined(ESP8266)
  yield();
#endif
  bool steep = _diff(y1, y0) > _diff(x1, x0);
  if (steep)
  {
    _swap_uint8_t(x0, y0);
    _swap_uint8_t(x1, y1);
  }

  if (x0 > x1)
  {
    _swap_uint8_t(x0, x1);
    _swap_uint8_t(y0, y1);
  }

  uint8_t dx, dy;
  dx = x1 - x0;
  dy = _diff(y1, y0);

  uint8_t err = dx / 2;
  int8_t ystep = (y0 < y1) ? 1 : -1;
  for (; x0 <= x1; x0++)
  {
    if (steep)
    {
      write_cache_point(y0, x0, color, save, actual_draw);
    }
    else
    {
      write_cache_point(x0, y0, color, save, actual_draw);
    }
    if (err < dy)
    {
      y0 += ystep;
      err += dx;
    }
    err -= dy;
  }
}

void write_cache_point(uint8_t x, uint8_t y, uint16_t color, bool save, bool actual_draw)
{
  for (int i = 0; i < cached_points_idx; i++)
  {
    if ((cached_points[i][0] == x) && (cached_points[i][1] == y))
    {
      return;
    }
  }
  if (save)
  {
    cached_points_idx++;
    cached_points[cached_points_idx][0] = x;
    cached_points[cached_points_idx][1] = y;
  }
  if (actual_draw)
  {
    tft->writePixel(x, y, color);
  }
}

void cache_line_points(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t points[][2], uint8_t array_size)
{
#if defined(ESP8266)
  yield();
#endif
  bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep)
  {
    _swap_uint8_t(x0, y0);
    _swap_uint8_t(x1, y1);
  }

  if (x0 > x1)
  {
    _swap_uint8_t(x0, x1);
    _swap_uint8_t(y0, y1);
  }

  uint8_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  uint8_t err = dx / 2;
  int8_t ystep = (y0 < y1) ? 1 : -1;
  int i = 0;

  for (; x0 <= x1; x0++)
  {
    if (steep)
    {
      points[i][0] = y0;
      points[i][1] = x0;
    }
    else
    {
      points[i][0] = x0;
      points[i][1] = y0;
    }
    if (err < dy)
    {
      y0 += ystep;
      err += dx;
    }
    err -= dy;
    i++;
  }
  for (; i < array_size; i++)
  {
    points[i][0] = 0;
    points[i][1] = 0;
  }
}

bool inCachedPoints(uint8_t x, uint8_t y, uint8_t points[][2], int array_size)
{
  for (int i = 0; i < array_size; i++)
  {
    if ((points[i][0] == x) && (points[i][1] == y))
    {
      return true;
    }
  }
  return false;
}

void redraw_hands_4_split(void (*redrawFunc)())
{
  if (inSplitRange(1, 1, CENTER, CENTER))
  {
    redrawFunc();
  }

  if (inSplitRange(CENTER + 1, 1, CENTER * 2, CENTER))
  {
    redrawFunc();
  }

  if (inSplitRange(1, CENTER + 1, CENTER, CENTER * 2))
  {
    redrawFunc();
  }

  if (inSplitRange(CENTER + 1, CENTER + 1, CENTER * 2, CENTER * 2))
  {
    redrawFunc();
  }
}

void spi_raw_overwrite_rect()
{
  tft->startWrite();
  tft->writeAddrWindow(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
  for (uint8_t y = yMin; y <= yMax; y++)
  {
    for (uint8_t x = xMin; x <= xMax; x++)
    {
      if (inLine(x, y, nsx, nsy, CENTER, CENTER))
      {
        tft->writeColor(SECOND_COLOR); // draw second hand
      }
      else if (inLine(x, y, nhx, nhy, CENTER, CENTER))
      {
        tft->writeColor(HOUR_COLOR); // draw hour hand
      }
      else if (inLine(x, y, nmx, nmy, CENTER, CENTER))
      {
        tft->writeColor(MINUTE_COLOR); // draw minute hand
      }
      else
      {
        tft->writeColor(BACKGROUND); // over write background color
      }
    }
  }
  tft->endWrite();
}

bool inSplitRange(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
  bool in_range = false;

  xMin = CENTER;
  yMin = CENTER;
  xMax = CENTER;
  yMax = CENTER;

  if (_ordered_in_range(ohx, x0, x1) && _ordered_in_range(ohy, y0, y1))
  {
    in_range = true;
    xMin = min(xMin, ohx);
    xMax = max(xMax, ohx);
    yMin = min(yMin, ohy);
    yMax = max(yMax, ohy);
  }

  if (_ordered_in_range(omx, x0, x1) && _ordered_in_range(omy, y0, y1))
  {
    in_range = true;
    xMin = min(xMin, omx);
    xMax = max(xMax, omx);
    yMin = min(yMin, omy);
    yMax = max(yMax, omy);
  }

  if (_ordered_in_range(osx, x0, x1) && _ordered_in_range(osy, y0, y1))
  {
    in_range = true;
    xMin = min(xMin, osx);
    xMax = max(xMax, osx);
    yMin = min(yMin, osy);
    yMax = max(yMax, osy);
  }

  if (_ordered_in_range(nhx, x0, x1) && _ordered_in_range(nhy, y0, y1))
  {
    in_range = true;
    xMin = min(xMin, nhx);
    xMax = max(xMax, nhx);
    yMin = min(yMin, nhy);
    yMax = max(yMax, nhy);
  }

  if (_ordered_in_range(nmx, x0, x1) && _ordered_in_range(nmy, y0, y1))
  {
    in_range = true;
    xMin = min(xMin, nmx);
    xMax = max(xMax, nmx);
    yMin = min(yMin, nmy);
    yMax = max(yMax, nmy);
  }

  if (_ordered_in_range(nsx, x0, x1) && _ordered_in_range(nsy, y0, y1))
  {
    in_range = true;
    xMin = min(xMin, nsx);
    xMax = max(xMax, nsx);
    yMin = min(yMin, nsy);
    yMax = max(yMax, nsy);
  }

  return in_range;
}

bool inLine(uint8_t x, uint8_t y, uint8_t lx0, uint8_t ly0, uint8_t lx1, uint8_t ly1)
{
  // range checking
  if (!_in_range(x, lx0, lx1))
  {
    return false;
  }
  if (!_in_range(y, ly0, ly1))
  {
    return false;
  }

  uint8_t dx = _diff(lx1, lx0);
  uint8_t dy = _diff(ly1, ly0);

  if (dy > dx)
  {
    _swap_uint8_t(dx, dy);
    _swap_uint8_t(x, y);
    _swap_uint8_t(lx0, ly0);
    _swap_uint8_t(lx1, ly1);
  }

  if (lx0 > lx1)
  {
    _swap_uint8_t(lx0, lx1);
    _swap_uint8_t(ly0, ly1);
  }

  uint8_t err = dx >> 1;
  int8_t ystep = (ly0 < ly1) ? 1 : -1;
  for (; lx0 <= x; lx0++)
  {
    if (err < dy)
    {
      ly0 += ystep;
      err += dx;
    }
    err -= dy;
  }

  return (y == ly0);
}
