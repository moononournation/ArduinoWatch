/*
   Arduino Watch Pro Micro Version
   - read current time from RTC
   - draw analog clock face every loop
   - shutdown power after 60 seconds
   - press power button to restart
*/
// real time clock class
// comment out if no RTC at all
// #define RTC_CLASS RTC_DS1307
#define RTC_CLASS RTC_DS3231
// #define RTC_CLASS RTC_PCF8523

// auto sleep time (in milliseconds)
// comment out if no need enter sleep, e.g. still in developing stage
#define SLEEP_TIME 20000L

#define BACKGROUND BLACK
#define MARK_COLOR WHITE
#define SUBMARK_COLOR DARKGREY // LIGHTGREY
#define HOUR_COLOR WHITE
#define MINUTE_COLOR BLUE // LIGHTGREY
#define SECOND_COLOR RED

#define HOUR_LEN 45
#define MINUTE_LEN 85
#define SECOND_LEN 100

#define SIXTIETH_DEGREE 6
#define TWELFTH_DEGREE 30
#define RIGHT_ANGLE_DEGREE 90
#define CENTER 120

#if defined(ESP32)
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 17
#define TFT_BL 22
#define WAKEUP_PIN 36
#else
#define TFT_CS 20
#define TFT_DC 19
#define TFT_RST 18
#define TFT_BL 10
#define WAKEUP_PIN 7
// #define RTC_EN_PIN 4
#endif
// #define LED_LEVEL 128

#include "Debug_Printer.h"
#include "IntTrigonometry.h"
#include <Arduino_GFX_Library.h>

#ifdef SLEEP_TIME
#if defined(ESP32)
#include <esp_sleep.h>
#elif defined(__AVR__)
#include <LowPower.h>
#endif
#endif // #ifdef SLEEP_TIME

#ifdef RTC_CLASS
#include <Wire.h>
#include <RTClib.h>
RTC_CLASS rtc;
#else
static uint8_t conv2d(const char *p)
{
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
// 1.3"/1.5" square IPS LCD 240x240
Arduino_ST7789 *tft = new Arduino_ST7789(bus, TFT_RST, 0 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
// 2.4" LCD
// Arduino_ST7789 *tft = new Arduino_ST7789(bus, TFT_RST, 1 /* rotation */);

static int isdeg, imdeg, ihdeg;
static uint8_t osx = CENTER, osy = CENTER, omx = CENTER, omy = CENTER, ohx = CENTER, ohy = CENTER; // Saved H, M, S x & y coords
static uint8_t nsx, nsy, nmx, nmy, nhx, nhy;                                                       // H, M, S x & y coords
static uint8_t xMin, yMin, xMax, yMax;                                                             // redraw range
static uint8_t hh, mm, ss;
static unsigned long targetTime, sleepTime; // next action time
#ifdef TFT_BL
static bool ledTurnedOn = false;
#endif
#ifdef DEBUG_MODE
static uint16_t loopCount = 0;
#endif

static uint8_t cached_points[(HOUR_LEN + 1 + MINUTE_LEN + 1 + SECOND_LEN + 1) * 2];
static int cached_points_idx = 0;
static uint8_t *last_cached_point;

void setup(void)
{
  DEBUG_BEGIN(115200);
  DEBUG_PRINTMLN(": start");

  tft->begin();
  DEBUG_PRINTMLN(": tft->begin()");

  tft->fillScreen(BACKGROUND);
  DEBUG_PRINTMLN(": tft->fillScreen(BACKGROUND)");

  pinMode(WAKEUP_PIN, INPUT_PULLUP);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
#endif

  // Draw 60 clock marks
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
  isdeg = (SIXTIETH_DEGREE * (cur_millis % 1000) / 1000) + (SIXTIETH_DEGREE * ss); // 0-59 (includes millis)
  nsx = iCos(isdeg - RIGHT_ANGLE_DEGREE) * SECOND_LEN / I_SCALE + CENTER;
  nsy = iSin(isdeg - RIGHT_ANGLE_DEGREE) * SECOND_LEN / I_SCALE + CENTER;
  if ((nsx != osx) || (nsy != osy))
  {
    imdeg = (isdeg / 60) + (SIXTIETH_DEGREE * mm); // 0-59 (includes seconds)
    ihdeg = (imdeg / 12) + (TWELFTH_DEGREE * hh);  // 0-11 (includes minutes)
    imdeg -= RIGHT_ANGLE_DEGREE;
    ihdeg -= RIGHT_ANGLE_DEGREE;
    nmx = iCos(imdeg) * MINUTE_LEN / I_SCALE + CENTER;
    nmy = iSin(imdeg) * MINUTE_LEN / I_SCALE + CENTER;
    nhx = iCos(ihdeg) * HOUR_LEN / I_SCALE + CENTER;
    nhy = iSin(ihdeg) * HOUR_LEN / I_SCALE + CENTER;

    // redraw hands
    // redraw_hands_cached_lines();
    redraw_hands_cached_draw_and_earse();

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

#ifdef DEBUG_MODE
    loopCount = 0;
#endif
  }
#endif // #ifdef SLEEP_TIME

  delay(1);
}

#ifdef TFT_BL
void backlight(bool enable)
{
  if (enable)
  {
#ifdef LED_LEVEL
#if defined(ESP32)
    ledcSetup(1, 12000, 8);   // 12 kHz PWM, 8-bit resolution
    ledcAttachPin(TFT_BL, 1); // assign TFT_BL pin to channel 1
    ledcWrite(1, LED_LEVEL);  // brightness 0 - 255
#else
    analogWrite(TFT_BL, LED_LEVEL);
#endif
#else
    digitalWrite(TFT_BL, HIGH);
#endif // #ifdef LED_LEVEL
    ledTurnedOn = true;
  }
  else
  {
    digitalWrite(TFT_BL, LOW);
    ledTurnedOn = false;
  }
}
#endif

#ifdef RTC_CLASS
void read_rtc()
{
#ifdef RTC_EN_PIN
  pinMode(RTC_EN_PIN, OUTPUT);
  digitalWrite(RTC_EN_PIN, HIGH);
#endif
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
#if not defined(ESP32)
  Wire.end();
#endif
}
#endif

#ifdef SLEEP_TIME
void enterSleep()
{
#ifdef TFT_BL
  backlight(false);
#endif
  tft->displayOff();

#if defined(ESP32)
  gpio_wakeup_enable((gpio_num_t)WAKEUP_PIN, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
#elif defined(__AVR__)
  // Allow wake up pin to trigger interrupt when the pin goes from high to low.
  attachInterrupt(digitalPinToInterrupt(WAKEUP_PIN), wakeUp, FALLING);

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

  targetTime = ((millis() / 1000) + 1) * 1000;

#ifdef SLEEP_TIME
  sleepTime = millis() + SLEEP_TIME;
#endif
}

#if defined(__AVR__)
void wakeUp()
{
  // Just a handler for the pin interrupt.
}
#endif
#endif // #ifdef SLEEP_TIME

void draw_round_clock_mark(uint8_t innerR1, uint8_t outerR1, uint8_t innerR2, uint8_t outerR2, uint8_t innerR3, uint8_t outerR3)
{
  int x, y;
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

    imdeg = (SIXTIETH_DEGREE * i) - RIGHT_ANGLE_DEGREE;
    x = iCos(imdeg);
    y = iSin(imdeg);
    x0 = x * outerR / I_SCALE + CENTER;
    y0 = y * outerR / I_SCALE + CENTER;
    x1 = x * innerR / I_SCALE + CENTER;
    y1 = y * innerR / I_SCALE + CENTER;

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
  int x, y;
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
      x = iTan(SIXTIETH_DEGREE * i);
      x0 = CENTER + (x * outerR / I_SCALE);
      y0 = CENTER + (1 - outerR);
      x1 = CENTER + (x * innerR / I_SCALE);
      y1 = CENTER + (1 - innerR);
    }
    else if (i < 23)
    {
      y = iTan((SIXTIETH_DEGREE * i) - RIGHT_ANGLE_DEGREE);
      x0 = CENTER + (outerR);
      y0 = CENTER + (y * outerR / I_SCALE);
      x1 = CENTER + (innerR);
      y1 = CENTER + (y * innerR / I_SCALE);
    }
    else if (i < 38)
    {
      x = iTan(SIXTIETH_DEGREE * i);
      x0 = CENTER - (x * outerR / I_SCALE);
      y0 = CENTER + (outerR);
      x1 = CENTER - (x * innerR / I_SCALE);
      y1 = CENTER + (innerR);
    }
    else if (i < 53)
    {
      y = iTan((SIXTIETH_DEGREE * i) - RIGHT_ANGLE_DEGREE);
      x0 = CENTER + (1 - outerR);
      y0 = CENTER - (y * outerR / I_SCALE);
      x1 = CENTER + (1 - innerR);
      y1 = CENTER - (y * innerR / I_SCALE);
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
  last_cached_point = cached_points;
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
  uint8_t *current_point = cached_points;
  for (int i = 0; i < cached_points_idx; i++)
  {
    if ((x == *(current_point++)) && (y == *(current_point)))
    {
      return;
    }
    current_point++;
  }
  if (save)
  {
    cached_points_idx++;
    *(last_cached_point++) = x;
    *(last_cached_point++) = y;
  }
  if (actual_draw)
  {
    tft->writePixel(x, y, color);
  }
}

void redraw_hands_cached_draw_and_earse()
{
  tft->startWrite();
  draw_and_earse_cached_line(CENTER, CENTER, nsx, nsy, SECOND_COLOR, cached_points, SECOND_LEN + 1, false, false);
  draw_and_earse_cached_line(CENTER, CENTER, nhx, nhy, HOUR_COLOR, cached_points + ((SECOND_LEN + 1) * 2), HOUR_LEN + 1, true, false);
  draw_and_earse_cached_line(CENTER, CENTER, nmx, nmy, MINUTE_COLOR, cached_points + ((SECOND_LEN + 1 + HOUR_LEN + 1) * 2), MINUTE_LEN + 1, true, true);
  tft->endWrite();
}

void draw_and_earse_cached_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color, uint8_t *cache, uint16_t cache_len, bool cross_check_second, bool cross_check_hour)
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

  uint8_t dx, dy;
  dx = _diff(x1, x0);
  dy = _diff(y1, y0);

  uint8_t err = dx / 2;
  int8_t xstep = (x0 < x1) ? 1 : -1;
  int8_t ystep = (y0 < y1) ? 1 : -1;
  x1 += xstep;
  uint8_t x, y, ox, oy;
  for (uint16_t i = 0; i <= dx; i++)
  {
    if (steep)
    {
      x = y0;
      y = x0;
    }
    else
    {
      x = x0;
      y = y0;
    }
    ox = *(cache + (i * 2));
    oy = *(cache + (i * 2) + 1);
    if ((x == ox) && (y == oy))
    {
      if (cross_check_second || cross_check_hour)
      {
        write_cache_pixel(x, y, color, cross_check_second, cross_check_hour);
      }
    }
    else
    {
      write_cache_pixel(x, y, color, cross_check_second, cross_check_hour);
      if ((ox > 0) || (oy > 0)) {
        write_cache_pixel(ox, oy, BACKGROUND, cross_check_second, cross_check_hour);
      }
      *(cache + (i * 2)) = x;
      *(cache + (i * 2) + 1) = y;
    }
    if (err < dy)
    {
      y0 += ystep;
      err += dx;
    }
    err -= dy;
    x0 += xstep;
  }
  for (uint16_t i = dx + 1; i < cache_len; i++)
  {
    ox = *(cache + (i * 2));
    oy = *(cache + (i * 2) + 1);
    if ((ox > 0) || (oy > 0))
    {
      write_cache_pixel(ox, oy, BACKGROUND, cross_check_second, cross_check_hour);
    }
    *(cache + (i * 2)) = 0;
    *(cache + (i * 2) + 1) = 0;
  }
}

void write_cache_pixel(uint8_t x, uint8_t y, uint16_t color, bool cross_check_second, bool cross_check_hour)
{
  uint8_t *cache = cached_points;
  if (cross_check_second)
  {
    for (int i = 0; i <= SECOND_LEN; i++)
    {
      if ((x == *(cache++)) && (y == *(cache)))
      {
        return;
      }
      cache++;
    }
  }
  if (cross_check_hour)
  {
    cache = cached_points + ((SECOND_LEN + 1) * 2);
    for (int i = 0; i <= HOUR_LEN; i++)
    {
      if ((x == *(cache++)) && (y == *(cache)))
      {
        return;
      }
      cache++;
    }
  }
  tft->writePixel(x, y, color);
}
