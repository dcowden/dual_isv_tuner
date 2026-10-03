#include <cassert>
#include <iostream>
#include "traffic_capture.h"

int main() {
    TrafficCapture<2> capture;
    capture.record(100, TrafficPath::PcToBoth, 0x00);
    capture.record(200, TrafficPath::PcToBoth, 0xFF);
    assert(capture.size() == 1);
    assert(capture.at(0).length == 2 && capture.at(0).bytes[1] == 0xFF);
    capture.record(300, TrafficPath::ADiscarded, 0xAA);
    assert(capture.size() == 2 && capture.at(1).path == TrafficPath::ADiscarded);
    capture.record(400, TrafficPath::BToPc, 0xBB);
    assert(capture.overwritten() == 1 && capture.at(0).path == TrafficPath::ADiscarded);
    assert(capture.at(1).bytes[0] == 0xBB);
    capture.record(5000, TrafficPath::BToPc, 0xCC); // Idle gap starts a new record.
    assert(capture.overwritten() == 2 && capture.at(1).length == 1);

    TrafficCapture<3> longRun;
    for (unsigned i = 0; i < 33; ++i) longRun.record(i, TrafficPath::AToPc, static_cast<uint8_t>(i));
    assert(longRun.size() == 2 && longRun.at(0).length == 32 && longRun.at(1).bytes[0] == 32);
    TrafficCapture<2> rollover;
    rollover.record(0xFFFFFFF0U, TrafficPath::BDiscarded, 1);
    rollover.record(10, TrafficPath::BDiscarded, 2);
    assert(rollover.size() == 1); // micros() rollover must not invent a long gap.
    std::cout << "Capture grouping, overflow, binary data, and timer rollover tests passed\n";
}
