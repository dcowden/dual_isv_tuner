#pragma once
#include <Arduino.h>

namespace diagnostics {
#ifdef ENABLE_WIFI_DIAGNOSTICS
void begin(HardwareSerial& pc, HardwareSerial& a, HardwareSerial& b);
void selector(bool a);
void pcByte(uint8_t byte, bool sentA, bool sentB);
void servoByte(bool fromA, uint8_t byte, bool selected, bool sentPc);
#else
inline void begin(HardwareSerial&, HardwareSerial&, HardwareSerial&) {}
inline void selector(bool) {}
inline void pcByte(uint8_t, bool, bool) {}
inline void servoByte(bool, uint8_t, bool, bool) {}
#endif
}
