#pragma once
#include <flint/app_api.h>

namespace flint::input {
struct TouchTransform {
    int width, height;
    bool swapXY{}, mirrorX{}, mirrorY{};
    bool map(int &x, int &y) const;
};

// Single-contact state, owned exclusively by the Core task. No input queue or allocation.
class TouchState {
public:
    using Post = flint::Error (*)(void *, flint::InputEvent);
    void sample(bool valid, bool down, int x, int y, uint64_t generation, uint64_t at, Post post,
        void *context);
    void flush(uint64_t generation, uint64_t at, Post post, void *context);
    bool pending() const {
        return cancel_;
    }

private:
    uint64_t generation_{};
    int16_t x_{}, y_{};
    bool contact_{}, delivered_{}, suppressed_{}, cancel_{};
    void cancel(uint64_t at, Post post, void *context);
};
} // namespace flint::input
