#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace flint::hardware {
struct RawTouch {
    uint64_t at{}, generation{};
    int x{}, y{};
    bool valid{}, down{};
};
// Caller supplies synchronization. Overflow invalidates the entire old gesture;
// the Core adapter must cancel before it interprets the retained records.
class TouchRecords {
public:
    void push(RawTouch sample) {
        if (count_ == records_.size()) {
            head_ = count_ = 0;
            overflow_ = true;
        }
        records_[(head_ + count_) % records_.size()] = sample;
        ++count_;
    }
    size_t drain(std::span<RawTouch> output, bool &lost) {
        lost = overflow_;
        overflow_ = false;
        size_t size{};
        while (count_ && size < output.size()) {
            output[size++] = records_[head_];
            head_ = (head_ + 1) % records_.size();
            --count_;
        }
        return size;
    }

private:
    std::array<RawTouch, 16> records_{};
    size_t head_{}, count_{};
    bool overflow_{};
};
} // namespace flint::hardware
