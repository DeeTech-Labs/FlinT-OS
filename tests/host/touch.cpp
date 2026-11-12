#include "input/touch_state.h"
#include <cstdlib>
#include <iostream>
#include <vector>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << __LINE__ << ": " #condition "\n";                                         \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
using namespace flint;
struct Sink {
    std::vector<InputEvent> events;
    Error result = Error::ok;
    static Error post(void *context, InputEvent event) {
        auto &sink = *static_cast<Sink *>(context);
        if (sink.result == Error::ok)
            sink.events.push_back(event);
        return sink.result;
    }
};
int main() {
    input::TouchTransform transform{280, 240, true, true, false};
    int x = 0, y = 279;
    CHECK(transform.map(x, y) && x == 0 && y == 0);
    x = 239;
    y = 0;
    CHECK(transform.map(x, y) && x == 279 && y == 239);
    x = 240;
    y = 0;
    CHECK(!transform.map(x, y));
    input::TouchState state;
    Sink sink;
    auto sample = [&](bool valid, bool down, int x, int y, uint64_t generation = 1) {
        state.sample(valid, down, x, y, generation, 10, Sink::post, &sink);
    };
    sample(true, true, 15, 20);
    sample(true, true, 15, 20); // stationary poll is not an event
    sample(true, true, 17, 22);
    sample(true, false, 0, 0); // release uses last valid coordinates
    CHECK(sink.events.size() == 3);
    CHECK(sink.events[0].kind == InputKind::touch_down);
    CHECK(sink.events[1].kind == InputKind::touch_move);
    CHECK(sink.events[2].kind == InputKind::touch_up && sink.events[2].x == 17);
    sample(true, true, 10, 10);
    sample(true, true, 12, 10, 2); // launch under a held finger
    sample(true, false, 0, 0, 2);
    CHECK(sink.events.size() == 4);
    sample(true, true, 10, 10, 2);
    CHECK(sink.events.back().generation == 2);
    sample(false, true, 0, 0, 2);
    CHECK(sink.events.back().kind == InputKind::touch_cancel);
    const auto count = sink.events.size();
    sample(true, true, 20, 20, 2);
    CHECK(sink.events.size() == count);
    sample(true, false, 0, 0, 2);
    sample(true, true, 10, 10, 2);
    sink.result = Error::quota_exceeded;
    sample(true, false, 0, 0, 2);
    CHECK(state.pending());
    const auto blocked = sink.events.size();
    sample(true, true, 40, 40, 2); // new contact while old cancel is still queued
    sink.result = Error::ok;
    state.flush(2, 11, Sink::post, &sink);
    CHECK(!state.pending() && sink.events.back().kind == InputKind::touch_cancel);
    sample(true, true, 50, 50, 2);
    CHECK(sink.events.size() == blocked + 1);
    sample(true, false, 0, 0, 2);
    sample(true, true, 10, 10, 2);
    sink.result = Error::quota_exceeded;
    sample(false, true, 0, 0, 2);
    CHECK(state.pending());
    sink.result = Error::ok;
    const auto before = sink.events.size();
    state.flush(3, 11, Sink::post, &sink);
    CHECK(!state.pending() && sink.events.size() == before);
    sample(true, false, 0, 0, 3);
    sink.result = Error::permission_denied;
    sample(true, true, 1, 2, 3);
    sink.result = Error::ok;
    sample(true, true, 3, 4, 3);
    CHECK(sink.events.size() == before); // failed down cannot become a move
    std::cout << "touch transforms, lifecycle, generation and backpressure: passed\n";
}
