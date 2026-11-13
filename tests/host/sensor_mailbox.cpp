#include "hardware/sensor_mailbox.h"
#include "hardware/button_state.h"
#include "hardware/rtc_time.h"
#include "hardware/touch_records.h"
#include "input/touch_state.h"
#include <cassert>
#include <vector>
using namespace flint;
static std::vector<InputKind> events;
static Error post(void *, InputEvent event) {
    events.push_back(event.kind);
    return Error::ok;
}
int main() {
    std::tm utc{};
    assert(hardware::rtcFromEpoch(1709251199, utc));
    assert(utc.tm_year == 124 && utc.tm_mon == 1 && utc.tm_mday == 29 && utc.tm_sec == 59);
    assert(hardware::rtcToEpoch(utc) == 1709251199);
    assert(hardware::rtcFromEpoch(1709251200, utc) && utc.tm_mon == 2 && utc.tm_mday == 1);
    assert(!hardware::rtcFromEpoch(946684799, utc));
    assert(!hardware::rtcFromEpoch(4102444800, utc));
    hardware::ButtonState button;
    button.edge(0);
    button.edge(10);
    button.maintain(true, 1, 1, 39, post, nullptr);
    assert(events.empty() && button.nextWake() == 40);
    button.maintain(true, 1, 1, 40, post, nullptr);
    assert(events.size() == 1 && events.back() == InputKind::press);
    button.maintain(true, 1, 1, 1039, post, nullptr);
    assert(events.size() == 1);
    button.maintain(true, 1, 1, 1040, post, nullptr);
    assert(events.size() == 2 && events.back() == InputKind::hold);
    button.maintain(true, 1, 2, 1041, post, nullptr); // generation switch suppresses old release
    button.edge(1100);
    button.maintain(false, 1, 2, 1130, post, nullptr);
    assert(events.size() == 2 && button.nextWake() == UINT64_MAX);
    events.clear();
    hardware::SensorMailbox mailbox;
    std::array<sensors::Demand, sensors::MaxSources + 5> desired{};
    for (size_t i = 0; i < desired.size(); ++i)
        desired[i].source = i + 1;
    mailbox.demand(desired);
    assert(mailbox.desiredCount == sensors::MaxSources);
    mailbox.demand({});
    assert(mailbox.desiredCount == 0 && mailbox.desired[0].source == 0);
    sensors::Sample sample{};
    sample.source = 1;
    sample.sampleId = 1;
    mailbox.publish(sample);
    sample.sampleId = 2;
    mailbox.publish(sample);
    std::array<sensors::Sample, 1> output{};
    assert(mailbox.poll(output) == 1 && output[0].sampleId == 2);
    assert(mailbox.poll(output) == 0);
    sample.source = 1;
    sample.sampleId = 1;
    mailbox.publish(sample);
    sample.source = 2;
    mailbox.publish(sample);
    assert(mailbox.poll(output) == 1 && output[0].source == 1);
    sample.sampleId = 2;
    assert(mailbox.publish(sample));
    assert(mailbox.poll(output) == 1 && output[0].source == 2 && output[0].sampleId == 2);
    assert(mailbox.poll(output) == 0); // A hole must not duplicate an already pending source.

    hardware::TouchRecords records;
    for (unsigned i = 0; i < 100; ++i)
        records.push({i, 41, 10, 20, true, true});
    std::array<hardware::RawTouch, 16> batch{};
    bool lost{};
    auto count = records.drain(batch, lost);
    assert(lost && count <= batch.size() && batch[count - 1].at == 99);
    input::TouchState state;
    state.sample(true, true, 10, 20, 41, 0, post, nullptr);
    if (lost)
        state.sample(false, false, 0, 0, 41, 1, post, nullptr);
    assert(events.size() == 2 && events.back() == InputKind::touch_cancel);
    for (size_t i = 0; i < count; ++i)
        state.sample(true, true, 10, 20, 41, 2, post, nullptr);
    assert(events.size() == 2); // overflow suppresses contact until release
    state.flush(42, 3, post, nullptr);
    // Old acquisition generation must never be reinterpreted by a new runtime.
    for (size_t i = 0; i < count; ++i) {
        assert(batch[i].generation == 41);
        if (batch[i].generation != 42)
            state.sample(false, false, 0, 0, 42, 3, post, nullptr);
    }
    assert(events.size() == 2);
    state.sample(true, false, 0, 0, 42, 4, post, nullptr);
    state.sample(true, true, 5, 6, 42, 5, post, nullptr);
    assert(events.size() == 3 && events.back() == InputKind::touch_down);
}
