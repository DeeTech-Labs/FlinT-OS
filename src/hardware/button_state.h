#pragma once
#include <algorithm>
#include <cstdint>
#include <flint/app_api.h>
namespace flint::hardware {
// Core-owned debounce state. Edge IRQ schedules sample at +30ms; no idle polling.
class ButtonState {
public:
    using Post = Error (*)(void *, InputEvent);
    void seed(bool down) {
        down_ = down;
    }
    void edge(uint64_t at) {
        readAt_ = at + 30;
    }
    bool due(uint64_t at) const {
        return at >= readAt_;
    }
    uint64_t nextWake() const {
        return std::min(readAt_, std::min(holdAt_, retryAt_));
    }
    void maintain(bool raw, int id, uint64_t generation, uint64_t at, Post post, void *context) {
        if (generation_ != generation) {
            delivered_ = false;
            holdAt_ = retryAt_ = UINT64_MAX;
        }
        if (at >= readAt_) {
            readAt_ = UINT64_MAX;
            if (raw != down_) {
                down_ = raw;
                if (down_ && retryAt_ == UINT64_MAX) {
                    generation_ = generation;
                    delivered_ =
                        post(context, {InputKind::press, id, generation, 0, 0, 0, at}) == Error::ok;
                    holdAt_ = delivered_ ? at + 1000 : UINT64_MAX;
                } else if (!down_) {
                    holdAt_ = UINT64_MAX;
                    if (delivered_)
                        retryAt_ = at;
                }
            }
        }
        if (at >= retryAt_) {
            const auto error = post(context, {InputKind::release, id, generation_, 0, 0, 0, at});
            if (error == Error::quota_exceeded)
                retryAt_ = at + 10;
            else {
                delivered_ = false;
                retryAt_ = UINT64_MAX;
            }
        }
        if (at >= holdAt_) {
            post(context, {InputKind::hold, id, generation_, 0, 0, 0, at});
            holdAt_ = UINT64_MAX;
        }
    }

private:
    bool down_{}, delivered_{};
    uint64_t generation_{}, readAt_{UINT64_MAX}, holdAt_{UINT64_MAX}, retryAt_{UINT64_MAX};
};
} // namespace flint::hardware
