#pragma once
#include <algorithm>
#include <flint/sensors.h>
namespace flint::hardware {
// Caller supplies synchronization. Latest desired state replaces the complete previous state;
// each physical source has one result slot, so a stalled Core cannot grow memory usage.
struct SensorMailbox {
    std::array<sensors::Demand, sensors::MaxSources> desired{};
    std::array<sensors::Sample, sensors::MaxSources> samples{};
    size_t desiredCount{};
    bool changed{};
    void demand(std::span<const sensors::Demand> input) {
        desired = {};
        desiredCount = std::min(input.size(), desired.size());
        std::copy_n(input.begin(), desiredCount, desired.begin());
        changed = true;
    }
    bool publish(const sensors::Sample &sample) {
        for (auto &slot : samples)
            if (slot.source == sample.source) {
                slot = sample;
                return true;
            }
        for (auto &slot : samples)
            if (!slot.source) {
                slot = sample;
                return false;
            }
        return false;
    }
    size_t poll(std::span<sensors::Sample> output) {
        size_t count{};
        for (auto &sample : samples)
            if (sample.source && count < output.size()) {
                output[count++] = sample;
                sample = {};
            }
        return count;
    }
};
} // namespace flint::hardware
