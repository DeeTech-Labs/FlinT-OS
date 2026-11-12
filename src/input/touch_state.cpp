#include "touch_state.h"
#include <utility>

namespace flint::input {
bool TouchTransform::map(int &x, int &y) const {
    if (swapXY)
        std::swap(x, y);
    if (width <= 0 || height <= 0 || width > 320 || height > 320 || x < 0 || y < 0 || x >= width ||
        y >= height)
        return false;
    if (mirrorX)
        x = width - 1 - x;
    if (mirrorY)
        y = height - 1 - y;
    return true;
}
void TouchState::flush(uint64_t generation, uint64_t at, Post post, void *context) {
    if ((delivered_ || cancel_) && generation_ != generation) {
        delivered_ = cancel_ = false;
        suppressed_ = contact_;
    }
    if (!cancel_)
        return;
    const auto error =
        post(context, {flint::InputKind::touch_cancel, 0, generation_, x_, y_, 0, at});
    // Only queue saturation is transient. Never deliver an old contact into a new runtime.
    if (error != flint::Error::quota_exceeded)
        cancel_ = false;
}
void TouchState::cancel(uint64_t at, Post post, void *context) {
    cancel_ |= delivered_;
    delivered_ = false;
    flush(generation_, at, post, context);
}
void TouchState::sample(bool valid, bool down, int x, int y, uint64_t generation, uint64_t at,
    Post post, void *context) {
    flush(generation, at, post, context);
    if (!valid) {
        cancel(at, post, context);
        suppressed_ = true;
        return;
    }
    if (!down) {
        if (delivered_) {
            const auto error =
                post(context, {flint::InputKind::touch_up, 0, generation_, x_, y_, 0, at});
            if (error != flint::Error::ok)
                cancel(at, post, context);
        }
        delivered_ = contact_ = suppressed_ = false;
        return;
    }
    if (suppressed_ || cancel_) {
        contact_ = true;
        suppressed_ = true;
        return;
    }
    if (contact_ && x == x_ && y == y_)
        return;
    const auto kind = contact_ ? flint::InputKind::touch_move : flint::InputKind::touch_down;
    if (!contact_)
        generation_ = generation;
    x_ = int16_t(x);
    y_ = int16_t(y);
    contact_ = true;
    const auto error = post(context, {kind, 0, generation_, x_, y_, 0, at});
    if (error == flint::Error::ok)
        delivered_ = true;
    else {
        cancel(at, post, context);
        suppressed_ = true;
    }
}
} // namespace flint::input
