#include <Arduino.h>
#include "diagnostics.h"

// All three serial links use 38400 baud, 8 data bits, no parity, 1 stop bit.
constexpr uint32_t BAUD = 38400;
constexpr size_t RX_BUFFER_SIZE = 1024;

// ESP32 GPIO numbers, not physical header pin positions.
constexpr int SERVO_A_RX = 17;  // MAX3233E R1OUT -> ESP32
constexpr int SERVO_A_TX = 16;  // ESP32 -> MAX3233E T1IN
constexpr int SERVO_B_RX = 19;  // MAX3233E R2OUT -> ESP32
constexpr int SERVO_B_TX = 18;  // ESP32 -> MAX3233E T2IN
constexpr int SELECT_PIN = 25; // Switch between GPIO25 and GND

HardwareSerial ServoA(1);
HardwareSerial ServoB(2);

void setup()
{
    pinMode(SELECT_PIN, INPUT_PULLUP);

    // The ESP32 Arduino core requires receive buffer sizes BEFORE begin().
    Serial.setRxBufferSize(RX_BUFFER_SIZE);
    ServoA.setRxBufferSize(RX_BUFFER_SIZE);
    ServoB.setRxBufferSize(RX_BUFFER_SIZE);

    // UART0 reaches the PC through the board's onboard USB-to-serial bridge.
    Serial.begin(BAUD, SERIAL_8N1);
    Serial.setDebugOutput(false);
    ServoA.begin(BAUD, SERIAL_8N1, SERVO_A_RX, SERVO_A_TX);
    ServoB.begin(BAUD, SERIAL_8N1, SERVO_B_RX, SERVO_B_TX);
    diagnostics::begin(Serial, ServoA, ServoB);
}

void loop()
{
    // PC -> BOTH servos, regardless of the selector position.
    while (Serial.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial.read());
        const bool sentA = (ServoA.write(b) == 1);
        const bool sentB = (ServoB.write(b) == 1);
        diagnostics::pcByte(b, sentA, sentB);
    }

    // Closed to GND = A; open (internal pullup) = B.
    // Change the switch only while communication is idle.
    const bool selectServoA = (digitalRead(SELECT_PIN) == LOW);
    diagnostics::selector(selectServoA);

    // Always drain both receivers; discard the unselected servo's bytes.
    while (ServoA.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(ServoA.read());
        const bool sentPc = selectServoA && (Serial.write(b) == 1);
        diagnostics::servoByte(true, b, selectServoA, sentPc);
    }

    while (ServoB.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(ServoB.read());
        const bool sentPc = !selectServoA && (Serial.write(b) == 1);
        diagnostics::servoByte(false, b, !selectServoA, sentPc);
    }
}
