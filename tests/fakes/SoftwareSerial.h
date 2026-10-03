#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace EspSoftwareSerial {
constexpr int SWSERIAL_8N1 = 0x800001c;

class UART {
public:
    std::deque<uint8_t> input;
    std::vector<uint8_t> output;
    bool started = false;
    uint32_t baud = 0;
    int rx = -1, tx = -1;
    size_t bufferCapacity = 0;

    void begin(uint32_t speed, int, int rxPin, int txPin, bool, size_t capacity = 64, size_t = 0) {
        started = true;
        baud = speed;
        rx = rxPin;
        tx = txPin;
        bufferCapacity = capacity;
    }
    int available() { return static_cast<int>(input.size()); }
    int read() {
        if (input.empty()) return -1;
        const int byte = input.front();
        input.pop_front();
        return byte;
    }
    size_t write(uint8_t byte) {
        output.push_back(byte);
        return 1;
    }
};
}
