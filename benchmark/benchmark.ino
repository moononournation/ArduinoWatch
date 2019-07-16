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
#define SIXTIETH_DEGREE 6
#define TWELFTH_DEGREE 30
#define RIGHT_ANGLE_DEGREE 90
#define CENTER 120

#ifdef ESP32
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 17
#else
#define TFT_DC 19
#define TFT_RST 18
#define TFT_BL 10
#define LED_LEVEL 128
#endif

#include "Debug_Printer.h"
#include "IntTrigonometry.h"
#include <SPI.h>
#include "Arduino_HWSPI.h"
#include "Arduino_GFX.h"
#include "Arduino_TFT.h"
#include "Arduino_ST7789.h" // Hardware-specific library for ST7789 (with or without CS pin)

//You can use different type of hardware initialization
#ifdef TFT_CS
Arduino_HWSPI *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
#else
Arduino_HWSPI *bus = new Arduino_HWSPI(TFT_DC); //for display without CS pin
#endif
Arduino_ST7789 *tft = new Arduino_ST7789(bus, TFT_RST, 2, 240, 240, 0, 80);

static float sdeg, mdeg, hdeg;
static int isdeg, imdeg, ihdeg;
static uint8_t osx = CENTER, osy = CENTER, omx = CENTER, omy = CENTER, ohx = CENTER, ohy = CENTER; // Saved H, M, S x & y coords
static uint8_t nsx, nsy, nmx, nmy, nhx, nhy;                                                       // H, M, S x & y coords
static uint8_t xMin, yMin, xMax, yMax;                                                             // redraw range

static uint8_t hh = 0, mm = 0, ss = 0;
static uint8_t tmp;
static uint16_t drawCount;

static uint8_t cached_points[HOUR_LEN + 1 + MINUTE_LEN + 1 + SECOND_LEN + 1][2];
static int cached_points_idx = 0;
static uint8_t cached_hour_points[HOUR_LEN + 1][2];
static uint8_t cached_minute_points[MINUTE_LEN + 1][2];
static uint8_t cached_second_points[SECOND_LEN + 1][2];

void setup(void)
{
  DEBUG_BEGIN(115200);
  DEBUG_PRINTMLN(": start");

#ifdef ESP32
  tft->begin(80000000);
#else
  tft->begin();
#endif
  DEBUG_PRINTMLN(": tft->begin()");
  tft->invertDisplay(true);

  tft->fillScreen(BACKGROUND);
  DEBUG_PRINTMLN(": tft->fillScreen(BACKGROUND)");

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, LED_LEVEL);
#endif

  //draw_round_clock_mark(104, 120, 112, 120, 114, 114);
  draw_square_clock_mark(102, 120, 108, 120, 114, 120);
  DEBUG_PRINTMLN(": Draw 60 clock marks");
}

void loop()
{
  unsigned long maxLoopMs = 0, minLoopMs = 9999, startMillis = millis();
  unsigned long cur_millis, cur_ms, i;

  DEBUG_PRINTMLN(": float version");
  for (i = 0; i < 60000L; i += 167) // Around 1 degree
  {
    cur_millis = millis();
    ss = i / 1000;
    sdeg = SIXTIETH_RADIAN * ((0.001 * (i % 1000)) + ss); // 0-59 (includes millis)
    mdeg = (SIXTIETH * sdeg) + (SIXTIETH_RADIAN * mm);    // 0-59 (includes seconds)
    hdeg = (TWELFTH * mdeg) + (TWELFTH_RADIAN * hh);      // 0-11 (includes minutes)
    sdeg -= RIGHT_ANGLE_RADIAN;
    mdeg -= RIGHT_ANGLE_RADIAN;
    hdeg -= RIGHT_ANGLE_RADIAN;
    nhx = cos(hdeg) * HOUR_LEN + CENTER;
    nhy = sin(hdeg) * HOUR_LEN + CENTER;
    nmx = cos(mdeg) * MINUTE_LEN + CENTER;
    nmy = sin(mdeg) * MINUTE_LEN + CENTER;
    nsx = cos(sdeg) * SECOND_LEN + CENTER;
    nsy = sin(sdeg) * SECOND_LEN + CENTER;
    cur_ms = millis() - cur_millis;
    maxLoopMs = max(maxLoopMs, cur_ms);
    minLoopMs = min(minLoopMs, cur_ms);
  }
  DEBUG_PRINTM(": maxLoopMs: ");
  DEBUG_PRINT(maxLoopMs);
  DEBUG_PRINT(", minLoopMs: ");
  DEBUG_PRINT(minLoopMs);
  DEBUG_PRINT(", TotalMs: ");
  DEBUG_PRINTLN(millis() - startMillis);

  maxLoopMs = 0, minLoopMs = 9999, startMillis = millis();
  DEBUG_PRINTMLN(": ifloat version");
  for (i = 0; i < 60000L; i += 167) // Around 1 degree
  {
    cur_millis = millis();
    ss = i / 1000;
    isdeg = (SIXTIETH_DEGREE * (i % 1000) / 1000) + (SIXTIETH_DEGREE * ss); // 0-59 (includes millis)
    imdeg = (isdeg / 60) + (SIXTIETH_DEGREE * mm);                          // 0-59 (includes seconds)
    ihdeg = (imdeg / 12) + (TWELFTH_DEGREE * hh);                           // 0-11 (includes minutes)
    isdeg -= RIGHT_ANGLE_DEGREE;
    imdeg -= RIGHT_ANGLE_DEGREE;
    ihdeg -= RIGHT_ANGLE_DEGREE;
    nhx = icos(ihdeg) * HOUR_LEN + CENTER;
    nhy = isin(ihdeg) * HOUR_LEN + CENTER;
    nmx = icos(imdeg) * MINUTE_LEN + CENTER;
    nmy = isin(imdeg) * MINUTE_LEN + CENTER;
    nsx = icos(isdeg) * SECOND_LEN + CENTER;
    nsy = isin(isdeg) * SECOND_LEN + CENTER;
    cur_ms = millis() - cur_millis;
    maxLoopMs = max(maxLoopMs, cur_ms);
    minLoopMs = min(minLoopMs, cur_ms);
  }
  DEBUG_PRINTM(": maxLoopMs: ");
  DEBUG_PRINT(maxLoopMs);
  DEBUG_PRINT(", minLoopMs: ");
  DEBUG_PRINT(minLoopMs);
  DEBUG_PRINT(", TotalMs: ");
  DEBUG_PRINTLN(millis() - startMillis);

  maxLoopMs = 0, minLoopMs = 9999, startMillis = millis();
  DEBUG_PRINTMLN(": int version");
  for (i = 0; i < 60000L; i += 167) // Around 1 degree
  {
    cur_millis = millis();
    ss = i / 1000;
    isdeg = (SIXTIETH_DEGREE * (i % 1000) / 1000) + (SIXTIETH_DEGREE * ss); // 0-59 (includes millis)
    imdeg = (isdeg / 60) + (SIXTIETH_DEGREE * mm);                          // 0-59 (includes seconds)
    ihdeg = (imdeg / 12) + (TWELFTH_DEGREE * hh);                           // 0-11 (includes minutes)
    isdeg -= RIGHT_ANGLE_DEGREE;
    imdeg -= RIGHT_ANGLE_DEGREE;
    ihdeg -= RIGHT_ANGLE_DEGREE;
    nhx = iCos(ihdeg) * HOUR_LEN / I_SCALE + CENTER;
    nhy = iSin(ihdeg) * HOUR_LEN / I_SCALE + CENTER;
    nmx = iCos(imdeg) * MINUTE_LEN / I_SCALE + CENTER;
    nmy = iSin(imdeg) * MINUTE_LEN / I_SCALE + CENTER;
    nsx = iCos(isdeg) * SECOND_LEN / I_SCALE + CENTER;
    nsy = iSin(isdeg) * SECOND_LEN / I_SCALE + CENTER;
    cur_ms = millis() - cur_millis;
    maxLoopMs = max(maxLoopMs, cur_ms);
    minLoopMs = min(minLoopMs, cur_ms);
  }
  DEBUG_PRINTM(": maxLoopMs: ");
  DEBUG_PRINT(maxLoopMs);
  DEBUG_PRINT(", minLoopMs: ");
  DEBUG_PRINT(minLoopMs);
  DEBUG_PRINT(", TotalMs: ");
  DEBUG_PRINTLN(millis() - startMillis);

  for (uint8_t test_idx = 0; test_idx < 10; test_idx++)
  {
    switch (test_idx)
    {
      case 0:
        DEBUG_PRINTMLN(": redraw_hands_erase_and_draw();");
        break;
      case 1:
        DEBUG_PRINTMLN(": redraw_hands_cached_lines();");
        break;
      case 2:
        DEBUG_PRINTMLN(": redraw_hands_rect(overwrite_rect_no_draw_float);");
        break;
      case 3:
        DEBUG_PRINTMLN(": redraw_hands_rect(overwrite_rect_no_draw_int);");
        break;
      case 4:
        DEBUG_PRINTMLN(": redraw_hands_rect(overwrite_rect_no_draw);");
        break;
      case 5:
        DEBUG_PRINTMLN(": redraw_hands_rect(overwrite_rect);");
        break;
      case 6:
        DEBUG_PRINTMLN(": redraw_hands_4_split(overwrite_rect);");
        break;
      case 7:
        DEBUG_PRINTMLN(": redraw_hands_4_split(draw_and_erase);");
        break;
      case 8:
        DEBUG_PRINTMLN(": redraw_hands_4_split(spi_raw_overwrite_rect);");
        break;
      case 9:
        DEBUG_PRINTMLN(": redraw_hands_4_split(spi_raw_cache_overwrite_rect);");
        break;
    }
    startMillis = millis();
    maxLoopMs = 0;
    minLoopMs = 9999;
    drawCount = 0;
    //for (i = 0; i < 60000L; i += 167) // Around 1 degree
    //for (i = 0; i < 60000L; i += 1000) // 6 degrees
    i = 0;
    while (i < 60000L)
    {
      cur_millis = millis() - startMillis;
      i = cur_millis;
      ss = i / 1000;

      sdeg = SIXTIETH_RADIAN * ((0.001 * (i % 1000)) + ss); // 0-59 (includes millis)
      //sdeg = SIXTIETH_RADIAN * ((0.001 * (i % 1000)) + ss); // 0-59 (includes millis)
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

        switch (test_idx)
        {
          case 0:
            redraw_hands_erase_and_draw();
            break;
          case 1:
            redraw_hands_cached_lines();
            break;
          case 2:
            redraw_hands_rect(overwrite_rect_no_draw_float);
            break;
          case 3:
            redraw_hands_rect(overwrite_rect_no_draw_int);
            break;
          case 4:
            redraw_hands_rect(overwrite_rect_no_draw);
            break;
          case 5:
            redraw_hands_rect(overwrite_rect);
            break;
          case 6:
            redraw_hands_4_split(overwrite_rect);
            break;
          case 7:
            redraw_hands_4_split(draw_and_erase);
            break;
          case 8:
            redraw_hands_4_split(spi_raw_overwrite_rect);
            break;
          case 9:
            cache_line_points(nsx, nsy, CENTER, CENTER, cached_second_points, SECOND_LEN + 1);
            cache_line_points(nhx, nhy, CENTER, CENTER, cached_hour_points, HOUR_LEN + 1);
            cache_line_points(nmx, nmy, CENTER, CENTER, cached_minute_points, MINUTE_LEN + 1);
            redraw_hands_4_split(spi_raw_cache_overwrite_rect);
            break;
        }
        ohx = nhx;
        ohy = nhy;
        omx = nmx;
        omy = nmy;
        osx = nsx;
        osy = nsy;

        unsigned long cur_ms = millis() - cur_millis - startMillis;
        maxLoopMs = max(maxLoopMs, cur_ms);
        minLoopMs = min(minLoopMs, cur_ms);
        drawCount++;
      }
    }
    DEBUG_PRINTM(": maxLoopMs: ");
    DEBUG_PRINT(maxLoopMs);
    DEBUG_PRINT(", minLoopMs: ");
    DEBUG_PRINT(minLoopMs);
    DEBUG_PRINT(", drawCount: ");
    DEBUG_PRINT(drawCount);
    DEBUG_PRINT(", TotalMs: ");
    DEBUG_PRINTLN(millis() - startMillis);
  }
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

void redraw_hands_erase_and_draw()
{
  // Erase 3 hands old positions
  tft->drawLine(ohx, ohy, CENTER, CENTER, BACKGROUND);
  tft->drawLine(omx, omy, CENTER, CENTER, BACKGROUND);
  tft->drawLine(osx, osy, CENTER, CENTER, BACKGROUND);
  // Draw 3 hands new positions
  tft->drawLine(nmx, nmy, CENTER, CENTER, MINUTE_COLOR);
  tft->drawLine(nhx, nhy, CENTER, CENTER, HOUR_COLOR);
  tft->drawLine(nsx, nsy, CENTER, CENTER, SECOND_COLOR);
}

void redraw_hands_rect(void (*redrawFunc)())
{
  xMin = min(min(min((uint8_t)CENTER, ohx), min(omx, osx)), min(min(nhx, nmx), nsx));
  xMax = max(max(max((uint8_t)CENTER, ohx), max(omx, osx)), max(max(nhx, nmx), nsx));
  yMin = min(min(min((uint8_t)CENTER, ohy), min(omy, osy)), min(min(nhy, nmy), nsy));
  yMax = max(max(max((uint8_t)CENTER, ohy), max(omy, osy)), max(max(nhy, nmy), nsy));

  redrawFunc();
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

void redraw_hands_cached_lines()
{
  cached_points_idx = 0;
  write_cached_line(nsx, nsy, CENTER, CENTER, SECOND_COLOR, false);
  write_cached_line(nhx, nhy, CENTER, CENTER, HOUR_COLOR, false);
  write_cached_line(nmx, nmy, CENTER, CENTER, MINUTE_COLOR, false);
  tft->startWrite();
  write_cached_line(omx, omy, CENTER, CENTER, BACKGROUND, true);
  tft->writeLine(nmx, nmy, CENTER, CENTER, MINUTE_COLOR);
  write_cached_line(ohx, ohy, CENTER, CENTER, BACKGROUND, true);
  tft->writeLine(nhx, nhy, CENTER, CENTER, HOUR_COLOR);
  write_cached_line(osx, osy, CENTER, CENTER, BACKGROUND, true);
  tft->writeLine(nsx, nsy, CENTER, CENTER, SECOND_COLOR);
  tft->endWrite();
}

void write_cached_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color, bool actual_draw)
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
      write_cache_point(y0, x0, color, actual_draw);
    }
    else
    {
      write_cache_point(x0, y0, color, actual_draw);
    }
    if (err < dy)
    {
      y0 += ystep;
      err += dx;
    }
    err -= dy;
  }
}

void write_cache_point(uint8_t x, uint8_t y, uint16_t color, bool actual_draw)
{
  for (int i = 0; i < cached_points_idx; i++)
  {
    if ((cached_points[i][0] == x) && (cached_points[i][1] == y))
    {
      return;
    }
  }
  if (actual_draw)
  {
    tft->writePixel(x, y, color);
  }
  else
  {
    cached_points_idx++;
    cached_points[cached_points_idx][0] = x;
    cached_points[cached_points_idx][1] = y;
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
  for (; i < array_size; i++) {
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

void overwrite_rect_no_draw_float()
{
  for (uint8_t y = yMin; y <= yMax; y++)
  {
    for (uint8_t x = xMin; x <= xMax; x++)
    {
      if (inLineInterFloat(x, y, nsx, nsy, CENTER, CENTER))
      {
        tmp++;
      }
      else if (inLineInterFloat(x, y, nhx, nhy, CENTER, CENTER))
      {
        tmp++;
      }
      else if (inLineInterFloat(x, y, nmx, nmy, CENTER, CENTER))
      {
        tmp++;
      }
    }
  }
}

void overwrite_rect_no_draw_int()
{
  for (uint8_t y = yMin; y <= yMax; y++)
  {
    for (uint8_t x = xMin; x <= xMax; x++)
    {
      if (inLineInterInt(x, y, nsx, nsy, CENTER, CENTER))
      {
        tmp++;
      }
      else if (inLineInterInt(x, y, nhx, nhy, CENTER, CENTER))
      {
        tmp++;
      }
      else if (inLineInterInt(x, y, nmx, nmy, CENTER, CENTER))
      {
        tmp++;
      }
    }
  }
}

void overwrite_rect_no_draw()
{
  for (uint8_t y = yMin; y <= yMax; y++)
  {
    for (uint8_t x = xMin; x <= xMax; x++)
    {
      if (inLine(x, y, nsx, nsy, CENTER, CENTER))
      {
        tmp++;
      }
      else if (inLine(x, y, nhx, nhy, CENTER, CENTER))
      {
        tmp++;
      }
      else if (inLine(x, y, nmx, nmy, CENTER, CENTER))
      {
        tmp++;
      }
    }
  }
}

void overwrite_rect()
{
  tft->setAddrWindow(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
  for (uint8_t y = yMin; y <= yMax; y++)
  {
    for (uint8_t x = xMin; x <= xMax; x++)
    {
      if (inLine(x, y, nsx, nsy, CENTER, CENTER))
      {
        tft->pushColor(SECOND_COLOR); // draw second hand
      }
      else if (inLine(x, y, nhx, nhy, CENTER, CENTER))
      {
        tft->pushColor(HOUR_COLOR); // draw hour hand
      }
      else if (inLine(x, y, nmx, nmy, CENTER, CENTER))
      {
        tft->pushColor(MINUTE_COLOR); // draw minute hand
      }
      else
      {
        tft->pushColor(BACKGROUND); // over write background color
      }
    }
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

void spi_raw_cache_overwrite_rect()
{
  tft->startWrite();
  tft->writeAddrWindow(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
  for (uint8_t y = yMin; y <= yMax; y++)
  {
    for (uint8_t x = xMin; x <= xMax; x++)
    {
      if (inCachedPoints(x, y, cached_second_points, SECOND_LEN + 1))
      {
        tft->writeColor(SECOND_COLOR); // draw second hand
      }
      else if (inCachedPoints(x, y, cached_hour_points, HOUR_LEN + 1))
      {
        tft->writeColor(HOUR_COLOR); // draw hour hand
      }
      else if (inCachedPoints(x, y, cached_minute_points, MINUTE_LEN + 1))
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

void draw_and_erase()
{
  tft->startWrite();
  for (uint8_t y = yMin; y <= yMax; y++)
  {
    for (uint8_t x = xMin; x <= xMax; x++)
    {
      if (inLine(x, y, nsx, nsy, CENTER, CENTER))
      {
        tft->writePixel(x, y, SECOND_COLOR); // draw second hand
      }
      else if (inLine(x, y, nhx, nhy, CENTER, CENTER))
      {
        tft->writePixel(x, y, HOUR_COLOR); // draw hour hand
      }
      else if (inLine(x, y, nmx, nmy, CENTER, CENTER))
      {
        tft->writePixel(x, y, MINUTE_COLOR); // draw minute hand
      }
      else if (
        (inLine(x, y, osx, osy, CENTER, CENTER)) || (inLine(x, y, ohx, ohy, CENTER, CENTER)) || (inLine(x, y, omx, omy, CENTER, CENTER)))
      {
        tft->writePixel(x, y, BACKGROUND); // erase with background color
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

bool inLineInterFloat(uint8_t x, uint8_t y, uint8_t lx0, uint8_t ly0, uint8_t lx1, uint8_t ly1)
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

  if (ly0 < ly1)
  {
    return (y == (ly0 + round((float)(x - lx0) * dy / dx)));
  }
  else
  {
    return (y == (ly0 - round((float)(x - lx0) * dy / dx)));
  }
}

bool inLineInterInt(uint8_t x, uint8_t y, uint8_t lx0, uint8_t ly0, uint8_t lx1, uint8_t ly1)
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

  if (ly0 < ly1)
  {
    return (y == (ly0 + ((x - lx0) * dy / dx)));
  }
  else
  {
    return (y == (ly0 - ((x - lx0) * dy / dx)));
  }
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
