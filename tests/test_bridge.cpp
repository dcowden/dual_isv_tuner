#include <cassert>
#include <iostream>
#include "Arduino.h"
int switchLevel = HIGH;
HardwareSerial Serial(0);
#include "main.cpp"

int main() {
    setup();
    assert(Serial.bufferBeforeBegin && ServoA.bufferBeforeBegin && ServoB.bufferBeforeBegin);
    assert(Serial.baud == 38400 && ServoA.baud == 38400 && ServoB.baud == 38400);
    assert(ServoA.rx == 17 && ServoA.tx == 16 && ServoB.rx == 19 && ServoB.tx == 18);
    for (int i = 0; i < 256; ++i) Serial.input.push_back(static_cast<uint8_t>(i));
    loop();
    assert(ServoA.output.size() == 256 && ServoB.output == ServoA.output);
    for (int i = 0; i < 256; ++i) assert(ServoA.output[i] == i);
    assert(Serial.output.empty()); // No debug text or local echo in the protocol.
    ServoA.input = {0x00, 0xAA, 0xFF}; ServoB.input = {0xBB, 0x01};
    switchLevel = LOW; loop();
    assert((Serial.output == std::vector<uint8_t>{0x00, 0xAA, 0xFF}));
    assert(ServoA.input.empty() && ServoB.input.empty());
    Serial.output.clear();
    switchLevel = HIGH; loop();
    assert(Serial.output.empty()); // Previously discarded B bytes must not reappear.
    ServoA.input = {0xCC}; ServoB.input = {0x00, 0xFF}; loop();
    assert((Serial.output == std::vector<uint8_t>{0x00, 0xFF}));
    assert(ServoA.input.empty() && ServoB.input.empty());
    std::cout << "Bridge routing tests passed\n";
}
