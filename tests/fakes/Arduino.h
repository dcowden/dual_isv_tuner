#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int INPUT_PULLUP = 2;
constexpr int SERIAL_8N1 = 0x800001c;
extern int switchLevel;
inline void pinMode(int, int) {}
inline int digitalRead(int) { return switchLevel; }

class HardwareSerial {
public:
    explicit HardwareSerial(int) {}
    std::deque<uint8_t> input;
    std::vector<uint8_t> output;
    bool started = false;
    bool bufferBeforeBegin = false;
    uint32_t baud = 0;
    int rx = -1, tx = -1;
    size_t setRxBufferSize(size_t size) { bufferBeforeBegin = !started; return size; }
    void begin(uint32_t speed, int, int rxPin = -1, int txPin = -1) {
        started = true; baud = speed; rx = rxPin; tx = txPin;
    }
    void setDebugOutput(bool) {}
    int available() { return static_cast<int>(input.size()); }
    int read() {
        if (input.empty()) return -1;
        const int b = input.front(); input.pop_front(); return b;
    }
    size_t write(uint8_t b) { output.push_back(b); return 1; }
};
extern HardwareSerial Serial;
