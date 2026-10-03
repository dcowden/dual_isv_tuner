#pragma once
#include <cstddef>
#include <cstdint>

enum class TrafficPath : uint8_t {
    PcToBoth, AToPc, BToPc, ADiscarded, BDiscarded, PcWriteFailed, AWriteFailed, BWriteFailed
};

struct TrafficRecord {
    uint32_t firstUs = 0;
    uint32_t lastUs = 0;
    TrafficPath path = TrafficPath::PcToBoth;
    uint8_t length = 0;
    uint8_t bytes[32] = {};
};

// Fixed memory, no allocation or I/O. Caller supplies synchronization.
template <size_t Capacity>
class TrafficCapture {
    static_assert(Capacity > 0, "Capture must contain at least one record");
public:
    void record(uint32_t nowUs, TrafficPath path, uint8_t byte) {
        if (count_ > 0) {
            TrafficRecord& tail = records_[(head_ + count_ - 1) % Capacity];
            if (tail.path == path && tail.length < sizeof(tail.bytes) &&
                static_cast<uint32_t>(nowUs - tail.lastUs) <= 2000) {
                tail.bytes[tail.length++] = byte;
                tail.lastUs = nowUs;
                return;
            }
        }
        if (count_ == Capacity) {
            head_ = (head_ + 1) % Capacity;
            --count_;
            ++overwritten_;
        }
        TrafficRecord& next = records_[(head_ + count_++) % Capacity];
        next.firstUs = next.lastUs = nowUs;
        next.path = path;
        next.length = 1;
        next.bytes[0] = byte;
    }
    size_t size() const { return count_; }
    const TrafficRecord& at(size_t index) const { return records_[(head_ + index) % Capacity]; }
    uint32_t overwritten() const { return overwritten_; }
private:
    TrafficRecord records_[Capacity] = {};
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t overwritten_ = 0;
};
