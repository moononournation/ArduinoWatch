#ifndef _DEBUG_PRINTER_H_
#define _DEBUG_PRINTER_H_

// Uncomment to enable printing out nice debug messages.
#define DEBUG_MODE

#ifdef DEBUG_MODE
// Define where debug output will be printed.
#define DEBUG_PRINTER Serial
#define DEBUG_BEGIN(...)              \
  {                                   \
    DEBUG_PRINTER.begin(__VA_ARGS__); \
    delay(1000);                      \
  }
#define DEBUG_PRINTM(...)             \
  {                                   \
    DEBUG_PRINTER.print(millis());    \
    DEBUG_PRINTER.print(__VA_ARGS__); \
  }
#define DEBUG_PRINT(...)              \
  {                                   \
    DEBUG_PRINTER.print(__VA_ARGS__); \
  }
#define DEBUG_PRINTMLN(...)             \
  {                                     \
    DEBUG_PRINTER.print(millis());      \
    DEBUG_PRINTER.println(__VA_ARGS__); \
  }
#define DEBUG_PRINTLN(...)              \
  {                                     \
    DEBUG_PRINTER.println(__VA_ARGS__); \
  }
#else
#define DEBUG_BEGIN(...) \
  {                      \
  }
#define DEBUG_PRINTM(...) \
  {                       \
  }
#define DEBUG_PRINT(...) \
  {                      \
  }
#define DEBUG_PRINTMLN(...) \
  {                         \
  }
#define DEBUG_PRINTLN(...) \
  {                        \
  }
#endif

#endif
