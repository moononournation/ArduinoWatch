/*
 * start revise from:
 * https://forum.arduino.cc/index.php?topic=69723.0
 */
#ifndef _INTTRIGONOMETRY_H_
#define _INTTRIGONOMETRY_H_

#ifdef __AVR__
 #include <avr/io.h>
 #include <avr/pgmspace.h>
#elif defined(ESP8266)
 #include <pgmspace.h>
#elif defined(__IMXRT1052__) || defined(__IMXRT1062__)
// PROGMEM is defefind for T4 to place data in specific memory section
 #undef PROGMEM
 #define PROGMEM
#else
 #define PROGMEM
#endif

#define I_SCALE         127

// 91 x 2 bytes ==> 182 bytes
static const int8_t iSinTable[] PROGMEM = {
0,
2,
4,
7,
9,
11,
13,
15,
18,
20,
22,
24,
26,
29,
31,
33,
35,
37,
39,
41,
43,
46,
48,
50,
52,
54,
56,
58,
60,
62,
64,
65,
67,
69,
71,
73,
75,
76,
78,
80,
82,
83,
85,
87,
88,
90,
91,
93,
94,
96,
97,
99,
100,
101,
103,
104,
105,
107,
108,
109,
110,
111,
112,
113,
114,
115,
116,
117,
118,
119,
119,
120,
121,
121,
122,
123,
123,
124,
124,
125,
125,
125,
126,
126,
126,
127,
127,
127,
127,
127,
127,
};

int iSin(int x)
{
  boolean pos = true;  // positive - keeps an eye on the sign.
  if (x < 0)
  {
    x = -x;
    pos = !pos;
  }
  while (x >= 360) {
    x -= 360;
  }
  if (x > 180)
  {
    x -= 180;
    pos = !pos;
  }
  if (x > 90) {
    x = 180 - x;
  }
  int8_t v = pgm_read_byte(&iSinTable[x]);
  if (pos) {
    return v;
  }
  return -v;
}

int iCos(int x)
{
  return iSin(x + 90);
}

int iTan(int x)
{
  return iSin(x) * I_SCALE / iCos(x);
}

#endif