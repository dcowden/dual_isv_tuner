#include <Arduino.h>
#include <SoftwareSerial.h>

// Tuner and servo links use 38400 baud, 8 data bits, no parity, 1 stop bit.
constexpr uint32_t TUNER_BAUD = 38400;
constexpr uint32_t DEBUG_BAUD = 115200;
constexpr size_t RX_BUFFER_SIZE = 1024;
constexpr size_t TUNER_RX_BUFFER_SIZE = 256;

// ESP32 GPIO numbers, not physical header pin positions.
constexpr int TUNER_RX = 21;   // Tuner TX -> ESP32
constexpr int TUNER_TX = 22;   // ESP32 -> tuner RX
constexpr int SERVO_A_TX = 17; // ESP32 -> MAX3233E T1IN
constexpr int SERVO_A_RX = 16; // MAX3233E R1OUT -> ESP32
constexpr int SERVO_B_RX = 19;  // MAX3233E R2OUT -> ESP32
constexpr int SERVO_B_TX = 18;  // ESP32 -> MAX3233E T2IN
constexpr int SELECT_PIN = 25; // Switch between GPIO25 and GND

HardwareSerial ServoA(1);
HardwareSerial ServoB(2);
EspSoftwareSerial::UART TunerSerial;

struct BridgeCounters {
    uint32_t tunerRx = 0;
    uint32_t tunerTx = 0;
    uint32_t aRx = 0;
    uint32_t aTx = 0;
    uint32_t bRx = 0;
    uint32_t bTx = 0;
    uint32_t discarded = 0;
    uint32_t writeFailures = 0;
};

BridgeCounters counters;

void printStatus()
{
    static uint32_t lastReportMs = 0;
    const uint32_t nowMs = millis();
    if (nowMs - lastReportMs < 1000) return;
    lastReportMs = nowMs;

    Serial.printf(
        "bridge %lu ms | tuner rx=%lu tx=%lu | A rx=%lu tx=%lu | B rx=%lu tx=%lu | discarded=%lu failed=%lu\n",
        static_cast<unsigned long>(nowMs),
        static_cast<unsigned long>(counters.tunerRx), static_cast<unsigned long>(counters.tunerTx),
        static_cast<unsigned long>(counters.aRx), static_cast<unsigned long>(counters.aTx),
        static_cast<unsigned long>(counters.bRx), static_cast<unsigned long>(counters.bTx),
        static_cast<unsigned long>(counters.discarded), static_cast<unsigned long>(counters.writeFailures));
}

void setup()
{
    pinMode(SELECT_PIN, INPUT_PULLUP);

    // HardwareSerial receive buffer sizes must be configured BEFORE begin().
    Serial.setRxBufferSize(RX_BUFFER_SIZE);
    ServoA.setRxBufferSize(RX_BUFFER_SIZE);
    ServoB.setRxBufferSize(RX_BUFFER_SIZE);

    // UART0 reaches the PC through the board's onboard USB-to-serial bridge.
    // It carries diagnostic output only; tuner protocol bytes never use UART0.
    Serial.begin(DEBUG_BAUD, SERIAL_8N1);
    Serial.setDebugOutput(false);
    TunerSerial.begin(TUNER_BAUD, EspSoftwareSerial::SWSERIAL_8N1, TUNER_RX, TUNER_TX,
                      false, TUNER_RX_BUFFER_SIZE);
    ServoA.begin(TUNER_BAUD, SERIAL_8N1, SERVO_A_RX, SERVO_A_TX);
    ServoB.begin(TUNER_BAUD, SERIAL_8N1, SERVO_B_RX, SERVO_B_TX);
    Serial.printf("dual-servo bridge ready | tuner RX=%d TX=%d at %lu 8N1\n",
                  TUNER_RX, TUNER_TX, static_cast<unsigned long>(TUNER_BAUD));
}

void loop()
{
    // Tuner -> BOTH servos, regardless of the selector position.
    while (TunerSerial.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(TunerSerial.read());
        const bool sentA = (ServoA.write(b) == 1);
        const bool sentB = (ServoB.write(b) == 1);
        ++counters.tunerRx;
        counters.aTx += sentA;
        counters.bTx += sentB;
        counters.writeFailures += !sentA;
        counters.writeFailures += !sentB;
    }

    // Closed to GND = A; open (internal pullup) = B.
    // Change the switch only while communication is idle.
    const bool selectServoA = (digitalRead(SELECT_PIN) == LOW);

    // Always drain both receivers; discard the unselected servo's bytes.
    while (ServoA.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(ServoA.read());
        const bool sentTuner = selectServoA && (TunerSerial.write(b) == 1);
        ++counters.aRx;
        if (selectServoA) {
            counters.tunerTx += sentTuner;
            counters.writeFailures += !sentTuner;
        } else {
            ++counters.discarded;
        }
    }

    while (ServoB.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(ServoB.read());
        const bool sentTuner = !selectServoA && (TunerSerial.write(b) == 1);
        ++counters.bRx;
        if (!selectServoA) {
            counters.tunerTx += sentTuner;
            counters.writeFailures += !sentTuner;
        } else {
            ++counters.discarded;
        }
    }

    printStatus();
}
