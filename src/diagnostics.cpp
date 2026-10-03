#include "diagnostics.h"

#ifdef ENABLE_WIFI_DIAGNOSTICS
#include <WiFi.h>
#include <WebServer.h>
#include <esp_system.h>
#include "traffic_capture.h"
#include "debug_page.h"

namespace diagnostics {
namespace {
constexpr char SSID[] = "DualServo-Debug";
constexpr char PASSWORD[] = "servo-debug";
struct State {
    TrafficCapture<256> capture;
    uint32_t pcRx = 0, aTx = 0, bTx = 0, aRx = 0, bRx = 0, pcTx = 0;
    uint32_t aDiscard = 0, bDiscard = 0, writeFailures = 0;
    uint32_t errors[3][6] = {}; // PC, A, B; indexed by hardwareSerial_error_t.
    bool selectedA = false;
    uint32_t lastLoopMs = 0;
};
State live;
State snapshot; // Only accessed by the web task; kept off its stack.
portMUX_TYPE stateLock = portMUX_INITIALIZER_UNLOCKED;
WebServer server(80);

void error(unsigned port, hardwareSerial_error_t value) {
    const unsigned index = static_cast<unsigned>(value);
    if (index == 0 || index >= 6) return;
    portENTER_CRITICAL(&stateLock);
    ++live.errors[port][index];
    portEXIT_CRITICAL(&stateLock);
}

const char* pathName(TrafficPath path) {
    switch (path) {
        case TrafficPath::PcToBoth: return "PC -> A+B";
        case TrafficPath::AToPc: return "A  -> PC";
        case TrafficPath::BToPc: return "B  -> PC";
        case TrafficPath::ADiscarded: return "A  discarded";
        case TrafficPath::BDiscarded: return "B  discarded";
        case TrafficPath::PcWriteFailed: return "PC TX FAILED";
        case TrafficPath::AWriteFailed: return "A->PC FAILED";
        case TrafficPath::BWriteFailed: return "B->PC FAILED";
    }
    return "unknown";
}

void sendCapture() {
    // Copy fixed-size state under a short lock. Formatting and all network I/O
    // happen after release, on core 0, separate from the Arduino bridge loop.
    portENTER_CRITICAL(&stateLock);
    snapshot = live;
    portEXIT_CRITICAL(&stateLock);

    String body;
    body.reserve(40000);
    char line[256];
    const uint32_t nowMs = millis();
    snprintf(line, sizeof(line),
        "DUAL SERVO DEBUG | build %s %s\nUptime: %lu ms | reset reason: %d | bridge heartbeat age: %lu ms\n",
        __DATE__, __TIME__, static_cast<unsigned long>(nowMs), static_cast<int>(esp_reset_reason()),
        static_cast<unsigned long>(nowMs - snapshot.lastLoopMs));
    body += line;
    body += "38400 baud, 8N1 | A RX=17 TX=16 | B RX=19 TX=18 | selector GPIO25\n";
    body += snapshot.selectedA ? "Selected reply: A (LOW)\n" : "Selected reply: B (HIGH)\n";
    snprintf(line, sizeof(line),
        "\nBYTE COUNTS SINCE BOOT\nPC RX: %lu     A TX: %lu     B TX: %lu\nA RX:  %lu     B RX: %lu     PC TX: %lu\nA discarded: %lu     B discarded: %lu     failed byte writes: %lu\n",
        static_cast<unsigned long>(snapshot.pcRx), static_cast<unsigned long>(snapshot.aTx),
        static_cast<unsigned long>(snapshot.bTx), static_cast<unsigned long>(snapshot.aRx),
        static_cast<unsigned long>(snapshot.bRx), static_cast<unsigned long>(snapshot.pcTx),
        static_cast<unsigned long>(snapshot.aDiscard), static_cast<unsigned long>(snapshot.bDiscard),
        static_cast<unsigned long>(snapshot.writeFailures));
    body += line;
    body += "\nUART ERROR EVENTS      break   RX buffer full   FIFO overflow   framing   parity\n";
    const char* names[] = {"PC", "A", "B"};
    for (unsigned p = 0; p < 3; ++p) {
        snprintf(line, sizeof(line), "%2s %25lu %12lu %15lu %9lu %8lu\n", names[p],
            static_cast<unsigned long>(snapshot.errors[p][UART_BREAK_ERROR]),
            static_cast<unsigned long>(snapshot.errors[p][UART_BUFFER_FULL_ERROR]),
            static_cast<unsigned long>(snapshot.errors[p][UART_FIFO_OVF_ERROR]),
            static_cast<unsigned long>(snapshot.errors[p][UART_FRAME_ERROR]),
            static_cast<unsigned long>(snapshot.errors[p][UART_PARITY_ERROR]));
        body += line;
    }
    snprintf(line, sizeof(line), "\nRECENT TRAFFIC | older rows overwritten: %lu\nFirst us     Last us      Route          HEX BYTES\n",
        static_cast<unsigned long>(snapshot.capture.overwritten()));
    body += line;
    for (size_t i = 0; i < snapshot.capture.size(); ++i) {
        const auto& record = snapshot.capture.at(i);
        snprintf(line, sizeof(line), "%10lu   %10lu   %-14s ",
            static_cast<unsigned long>(record.firstUs), static_cast<unsigned long>(record.lastUs), pathName(record.path));
        body += line;
        for (uint8_t j = 0; j < record.length; ++j) {
            snprintf(line, sizeof(line), "%02X ", static_cast<unsigned>(record.bytes[j]));
            body += line;
        }
        body += '\n';
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain", body);
}

void webTask(void*) {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    const IPAddress address(192, 168, 4, 1);
    // Retry AP startup without interrupting serial forwarding.
    while (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0)) ||
           !WiFi.softAP(SSID, PASSWORD)) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", DEBUG_PAGE); });
    server.on("/capture.txt", HTTP_GET, sendCapture);
    server.begin();
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
} // namespace

void begin(HardwareSerial& pc, HardwareSerial& a, HardwareSerial& b) {
    pc.onReceiveError([](hardwareSerial_error_t e) { error(0, e); });
    a.onReceiveError([](hardwareSerial_error_t e) { error(1, e); });
    b.onReceiveError([](hardwareSerial_error_t e) { error(2, e); });
    // All three hardware UARTs are already in use. Diagnostics use Wi-Fi only.
    // Failure to allocate a debug task leaves forwarding operational.
    xTaskCreatePinnedToCore(webTask, "servo-debug", 6144, nullptr, 1, nullptr, 0);
}

void selector(bool a) {
    const uint32_t nowMs = millis();
    portENTER_CRITICAL(&stateLock);
    live.selectedA = a;
    live.lastLoopMs = nowMs;
    portEXIT_CRITICAL(&stateLock);
}

void pcByte(uint8_t byte, bool sentA, bool sentB) {
    const uint32_t nowUs = micros();
    portENTER_CRITICAL(&stateLock);
    ++live.pcRx;
    live.aTx += sentA;
    live.bTx += sentB;
    live.writeFailures += !sentA;
    live.writeFailures += !sentB;
    live.capture.record(nowUs, sentA && sentB ? TrafficPath::PcToBoth : TrafficPath::PcWriteFailed, byte);
    portEXIT_CRITICAL(&stateLock);
}

void servoByte(bool fromA, uint8_t byte, bool selected, bool sentPc) {
    const uint32_t nowUs = micros();
    portENTER_CRITICAL(&stateLock);
    ++(fromA ? live.aRx : live.bRx);
    if (selected) {
        live.pcTx += sentPc;
        live.writeFailures += !sentPc;
    } else {
        ++(fromA ? live.aDiscard : live.bDiscard);
    }
    const TrafficPath path = !selected ? (fromA ? TrafficPath::ADiscarded : TrafficPath::BDiscarded) :
        sentPc ? (fromA ? TrafficPath::AToPc : TrafficPath::BToPc) :
        (fromA ? TrafficPath::AWriteFailed : TrafficPath::BWriteFailed);
    live.capture.record(nowUs, path, byte);
    portEXIT_CRITICAL(&stateLock);
}
} // namespace diagnostics
#endif
